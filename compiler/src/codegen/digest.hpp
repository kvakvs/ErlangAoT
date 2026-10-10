#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace clause::codegen {
// The MD5 digest of the bytes (LLVM's implementation, kept behind codegen).
std::array<std::uint8_t, 16> md5(std::string_view bytes);
} // namespace clause::codegen
