#include "integers.hpp"

namespace erlang_aot::runtime::detail {
namespace {
// Digits consumed per pass over the limbs: 10^9 times a 32-bit half limb plus a carry fits in 64 bits.
constexpr std::size_t CHUNK_DIGITS = 9;

// Multiply a limb by scale and add carry, splitting a 64-bit limb into halves so no compiler-specific wide integer
// is needed; return the carry out.
Word accumulate(Word &limb, std::uint64_t scale, Word carry) {
    constexpr std::uint64_t mask = 0xffffffff;
    const auto low = (static_cast<std::uint64_t>(limb) & mask) * scale + carry;
    if constexpr (sizeof(Word) == 4) {
        limb = static_cast<Word>(low);
        return static_cast<Word>(low >> 32);
    } else {
        const auto high = (static_cast<std::uint64_t>(limb) >> 32) * scale + (low >> 32);
        limb = static_cast<Word>((high << 32) | (low & mask));
        return static_cast<Word>(high >> 32);
    }
}
} // namespace

Integer integer_digits(std::string_view digits) {
    std::vector<Word> limbs(1, 0);
    for (std::size_t at = 0; at < digits.size(); at += CHUNK_DIGITS) {
        const auto chunk = digits.substr(at, CHUNK_DIGITS);
        std::uint64_t scale = 1;
        Word carry = 0;
        for (const char digit : chunk) {
            scale *= 10;
            carry = carry * 10 + static_cast<unsigned>(digit - '0');
        }
        for (auto &limb : limbs) {
            carry = accumulate(limb, scale, carry);
        }
        if (carry != 0) {
            limbs.push_back(carry);
        }
    }
    return integer_words(limbs, false);
}
} // namespace erlang_aot::runtime::detail
