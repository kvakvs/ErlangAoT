#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <string_view>

namespace erlang_aot::project {
struct NewProjectFilename {
    // Keep a requested filename distinct from its invocation-directory base.
    std::filesystem::path value;
};

struct CreationIO {
    // Tests can replace writing/closing while retaining real exclusive file creation.
    std::function<void(std::ofstream &, std::string_view)> write;
    std::function<void(std::ofstream &)> close;
};

// Check filename structure without filesystem access or path normalization.
bool valid_creation_filename(const std::filesystem::path &filename);
// Validate a destination filename and append .toml without replacing other extensions.
std::filesystem::path creation_path(const std::filesystem::path &base, const NewProjectFilename &filename);
// Exclusively create a complete annotated starter; never overwrite an existing destination.
std::filesystem::path create_project(const std::filesystem::path &base, const NewProjectFilename &filename,
                                     const CreationIO &io = {});
} // namespace erlang_aot::project
