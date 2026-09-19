#pragma once
#include "model.hpp"

namespace erlang_aot::project {
// Identify an existing filesystem object, following aliases without case folding.
std::string file_identity(const std::filesystem::path &path, const Site &site);
} // namespace erlang_aot::project
