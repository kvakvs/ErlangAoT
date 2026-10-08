#include "sdk.hpp"
#include <llvm/Config/llvm-config.h>

namespace clause::codegen {
std::string_view sdk_version() { return LLVM_VERSION_STRING; }
} // namespace clause::codegen
