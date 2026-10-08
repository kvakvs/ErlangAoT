#include "paths.hpp"
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>

namespace clause::artifacts {
namespace {
// Reserve a private staging directory atomically so concurrent compiler invocations never share files.
std::filesystem::path staging_directory(const std::filesystem::path &root) {
    std::filesystem::create_directories(root);
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto path = root / (".clause-stage-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(path)) {
            return path;
        }
    }
    throw std::runtime_error("cannot reserve artifact staging directory");
}

struct Staging {
    // Own only our reserved directory and explicitly created temporary files, never destination files.
    std::filesystem::path directory;
    std::vector<std::filesystem::path> files;

    // Remove individual owned paths, including partially written files, without recursive cleanup.
    ~Staging() {
        std::error_code ignored;
        for (const auto &file : files) {
            std::filesystem::remove(file, ignored);
        }
        std::filesystem::remove(directory, ignored);
    }
};

// Detect write, flush and close errors before allowing any destination replacement.
void write(std::ofstream &file, const codegen::OutputBuffer &output, const PublicationIO &io) {
    if (output.bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("artifact exceeds supported stream size");
    }
    if (io.write) {
        io.write(file, output.bytes);
    } else {
        file.write(reinterpret_cast<const char *>(output.bytes.data()),
                   static_cast<std::streamsize>(output.bytes.size()));
    }
    file.flush();
    if (io.close) {
        io.close(file);
    } else {
        file.close();
    }
    if (!file) {
        throw std::runtime_error("artifact write or close failed");
    }
}

// Record exclusive creation before writing so exceptions remove partially staged bytes.
void stage(Staging &staging, const detail::Destination &destination, const PublicationIO &io) {
    const auto path = staging.directory / destination.path.filename();
    staging.files.push_back(path);
    std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!file) {
        staging.files.pop_back();
        throw std::runtime_error("cannot create staged artifact");
    }
    write(file, *destination.output, io);
}
} // namespace

void publish(const std::span<const codegen::OutputBuffer> outputs, const std::filesystem::path &root,
             const std::span<const std::filesystem::path> inputs, const std::string_view object_extension,
             const PublicationIO &io) {
    const auto destinations = detail::plan(outputs, root, inputs, object_extension);
    if (destinations.empty()) {
        return;
    }
    Staging staging{staging_directory(destinations.front().path.parent_path()), {}};
    for (const auto &destination : destinations) {
        stage(staging, destination, io);
    }
    for (std::size_t i = 0; i < destinations.size(); ++i) {
        detail::validate_destination(destinations[i].path, inputs);
        detail::replace(staging.files[i], destinations[i].path);
    }
}
} // namespace clause::artifacts
