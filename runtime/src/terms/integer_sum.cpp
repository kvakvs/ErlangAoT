#include "integers.hpp"
#include <algorithm>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
// Export absolute magnitude without borrowing Boost's platform-specific limb representation.
std::vector<Word> magnitude(const Integer &value) {
    std::vector<Word> words(
        std::max<std::size_t>(1, (integer_bits(value) + sizeof(Word) * 8 - 1) / (sizeof(Word) * 8)));
    boost::multiprecision::export_bits(value, words.data(), sizeof(Word) * 8, false);
    return words;
}

// Unsigned wrap is defined; comparison recovers both carries without a wider native type.
Word add(Word left, Word right, Word &carry) {
    const Word sum = left + right;
    const Word result = sum + carry;
    carry = static_cast<Word>(sum < left || result < sum);
    return result;
}

// The caller supplies the larger magnitude first, so the final borrow is always zero.
Word subtract(Word left, Word right, Word &borrow) {
    const Word difference = left - right;
    const Word result = difference - borrow;
    borrow = static_cast<Word>(left < right || difference < borrow);
    return result;
}

// Compare high words first; no allocation or arbitrary-integer arithmetic is needed for sign selection.
bool less(const std::vector<Word> &left, const std::vector<Word> &right) {
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    return std::lexicographical_compare(left.rbegin(), left.rend(), right.rbegin(), right.rend());
}

// One extra word holds addition carry; import normalizes unused high zeros.
Integer combine(std::vector<Word> left, const std::vector<Word> &right, bool addition, bool negative) {
    constexpr auto limb_limit = (integer_bit_limit + sizeof(Word) * 8 - 1) / (sizeof(Word) * 8);
    if (left.size() > limb_limit || right.size() > limb_limit) {
        throw std::length_error("integer operand limit exceeded");
    }
    left.resize(std::max(left.size(), right.size()) + 1);
    Word carry = 0;
    const auto operation = addition ? add : subtract;
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = operation(left[i], i < right.size() ? right[i] : 0, carry);
    }
    return integer_words(left, negative);
}
} // namespace

Integer integer_sum(const Integer &left, const Integer &right, bool subtracting) {
    auto lhs = magnitude(left);
    auto rhs = magnitude(right);
    bool negative = left < 0;
    const bool addition = negative == ((right < 0) != subtracting);
    if (!addition && less(lhs, rhs)) {
        lhs.swap(rhs);
        negative = !negative;
    }
    return combine(std::move(lhs), rhs, addition, negative);
}
} // namespace erlang_aot::runtime::detail
