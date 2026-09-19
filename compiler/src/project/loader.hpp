#pragma once
#include "model.hpp"
#include <functional>
#include <toml++/toml.hpp>

namespace erlang_aot::project {
struct Document {
    // Keep TOML ownership private to decoding; manifest models copy all values.
    toml::table table;
    std::filesystem::path file;
};

// Replace only file reading in tests while keeping parsing and diagnostics real.
using Reader = std::function<std::string(const std::filesystem::path &, std::size_t)>;
// Parse bounded TOML text with stable manifest identity and located errors.
Document parse_document(std::string_view bytes, const std::filesystem::path &file, const Limits &limits = {});
// Load a bounded regular file using native paths or an injected reader.
Document load(const std::filesystem::path &file, const Limits &limits = {}, const Reader &reader = {});
} // namespace erlang_aot::project
