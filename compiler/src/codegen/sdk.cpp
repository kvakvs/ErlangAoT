#include "sdk.hpp"
#include <llvm/Config/llvm-config.h>

namespace erlang_aot::codegen {
std::string_view sdk_version() { return LLVM_VERSION_STRING; }
} // namespace erlang_aot::codegen
