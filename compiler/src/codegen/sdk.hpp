#pragma once
#include <string_view>

namespace erlang_aot::codegen {
// Identify the SDK used to build the backend without exposing LLVM headers to callers.
std::string_view sdk_version();
} // namespace erlang_aot::codegen
