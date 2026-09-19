#include "create.hpp"
#include "diagnostics.hpp"
#include "identity.hpp"
#include "paths.hpp"
#include "template.hpp"
#include <algorithm>

namespace erlang_aot::project {
namespace {
// Fold only ASCII suffix letters without locale-sensitive filename changes.
char lower(char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value; }

// Write the complete bounded template and detect both write and close failures.
void write_template(std::ofstream &output, std::string_view text, const CreationIO &io) {
    if (io.write) {
        io.write(output, text);
    } else {
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (!output) {
        throw std::runtime_error("manifest write failed");
    }
    if (io.close) {
        io.close(output);
    } else {
        output.close();
    }
    if (output.is_open() || !output) {
        throw std::runtime_error("manifest close failed");
    }
}

// Remove only a destination still identified as this invocation's incomplete file.
bool cleanup(std::ofstream &output, const std::filesystem::path &path, const std::string &identity, const Site &site) {
    try {
        if (output.is_open()) {
            output.clear();
            output.close();
        }
        if (identity.empty() || file_identity(path, site) != identity) {
            return false;
        }
        return std::filesystem::remove(path);
    } catch (const std::exception &) {
        return false;
    }
}
} // namespace

bool valid_creation_filename(const std::filesystem::path &filename) {
    const auto name = filename.filename();
    return !filename.empty() && !name.empty() && name != "." && name != "..";
}

std::filesystem::path creation_path(const std::filesystem::path &base, const NewProjectFilename &filename) {
    const auto name = filename.value.filename();
    if (!valid_creation_filename(filename.value)) {
        fail({filename.value, {}, {}, 0, 0}, "--new-project requires a filename", 2);
    }
    auto result = filename.value;
    auto extension = path_text(name);
    std::ranges::transform(extension, extension.begin(), lower);
    if (!extension.ends_with(".toml")) {
        result += ".toml";
    }
    return absolute_path(base, result);
}

std::filesystem::path create_project(const std::filesystem::path &base, const NewProjectFilename &filename,
                                     const CreationIO &io) {
    const auto path = creation_path(base, filename);
    const Site site{path, {}, {}, 0, 0};
    const auto text = starter_template();
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::noreplace);
    if (!output) {
        fail(site, "cannot create manifest exclusively; destination exists or parent is not writable");
    }
    std::string identity;
    try {
        identity = file_identity(path, site);
        write_template(output, text, io);
    } catch (const std::exception &error) {
        const bool removed = cleanup(output, path, identity, site);
        const auto suffix = removed ? "" : "; incomplete manifest cleanup could not be verified";
        fail(site, std::string(error.what()) + suffix);
    }
    return path;
}
} // namespace erlang_aot::project
