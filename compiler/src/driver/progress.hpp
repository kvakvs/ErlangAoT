#pragma once
#include "../codegen/request.hpp"

namespace erlang_aot::cli {
struct FrontendRequest;
// Capture immutable project context and keep every backend trace on stderr.
codegen::ProgressCallback progress_callback(const FrontendRequest &request);
} // namespace erlang_aot::cli
