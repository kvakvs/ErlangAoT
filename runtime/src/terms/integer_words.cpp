#include "integers.hpp"
#include <stdexcept>

namespace erlang_aot::runtime::detail {
Integer integer_words(std::span<const Word> words, bool negative) {
    constexpr auto limit = (integer_bit_limit + sizeof(Word) * 8 - 1) / (sizeof(Word) * 8) + 1;
    if (words.size() > limit) {
        throw std::length_error("integer word limit exceeded");
    }
    if (words.empty()) {
        return Integer{0};
    }
    Integer result;
    const auto *begin = words.data();
    boost::multiprecision::import_bits(result, begin, begin + words.size(), sizeof(Word) * 8, false);
    if (negative) {
        result.backend().negate();
    }
    return result;
}
} // namespace erlang_aot::runtime::detail
