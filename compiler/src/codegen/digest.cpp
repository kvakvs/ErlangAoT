#include "digest.hpp"
#include <algorithm>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/MD5.h>

namespace clause::codegen {
std::array<std::uint8_t, 16> md5(const std::string_view bytes) {
    const auto digest = llvm::MD5::hash(
        llvm::ArrayRef<std::uint8_t>(reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
    std::array<std::uint8_t, 16> result{};
    std::ranges::copy(digest, result.begin());
    return result;
}
} // namespace clause::codegen
