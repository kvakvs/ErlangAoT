#include "loader.hpp"
#include "diagnostics.hpp"
#include <array>
#include <fstream>

namespace erlang_aot::project {
namespace {
// Bound allocation during reading instead of checking only after loading a file.
std::string read_bounded(const std::filesystem::path &file, std::size_t limit) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        fail({file, {}, {}, 0, 0}, "cannot read manifest: not an accessible regular file");
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        fail({file, {}, {}, 0, 0}, "cannot open manifest");
    }
    std::string result;
    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - result.size()) {
            fail({file, {}, {}, 0, 0}, "manifest byte limit exceeded");
        }
        result.append(buffer.data(), count);
    }
    if (!input.eof()) {
        fail({file, {}, {}, 0, 0}, "cannot read manifest");
    }
    return result;
}
} // namespace

Document parse_document(std::string_view bytes, const std::filesystem::path &file, const Limits &limits) {
    if (bytes.size() > limits.manifest_bytes) {
        fail({file, {}, {}, 0, 0}, "manifest byte limit exceeded");
    }
    try {
        const auto name = file.generic_u8string();
        return {toml::parse(bytes, std::string(name.begin(), name.end())), file};
    } catch (const toml::parse_error &error) {
        fail({file, {}, {}, error.source().begin.line, error.source().begin.column}, std::string(error.description()));
    }
}

Document load(const std::filesystem::path &file, const Limits &limits, const Reader &reader) {
    try {
        const auto bytes = reader ? reader(file, limits.manifest_bytes) : read_bounded(file, limits.manifest_bytes);
        return parse_document(bytes, file, limits);
    } catch (const Failure &) {
        throw;
    } catch (const std::exception &error) {
        fail({file, {}, {}, 0, 0}, "cannot read manifest: " + std::string(error.what()));
    }
}
} // namespace erlang_aot::project
