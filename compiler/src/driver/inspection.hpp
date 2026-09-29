#pragma once
#include "../codegen/compilation.hpp"
#include "frontend.hpp"

namespace erlang_aot::cli {
// Print requested verified snapshots, stopping before object emission and filesystem publication.
bool inspect_ir(codegen::Compilation &compilation, const FrontendRequest &request);
} // namespace erlang_aot::cli
