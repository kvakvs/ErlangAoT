#include "integers.hpp"

namespace erlang_aot::runtime::detail {
namespace {
// Split a 64-bit limb into halves so multiplying by ten needs no compiler-specific wide integer.
Word accumulate(Word &limb, Word carry) {
    constexpr std::uint64_t mask = 0xffffffff;
    const auto low = (static_cast<std::uint64_t>(limb) & mask) * 10 + carry;
    if constexpr (sizeof(Word) == 4) {
        limb = static_cast<Word>(low);
        return static_cast<Word>(low >> 32);
    } else {
        const auto high = (static_cast<std::uint64_t>(limb) >> 32) * 10 + (low >> 32);
        limb = static_cast<Word>((high << 32) | (low & mask));
        return static_cast<Word>(high >> 32);
    }
}
} // namespace

Integer integer_digits(std::string_view digits) {
    std::vector<Word> limbs(1, 0);
    for (const char digit : digits) {
        Word carry = static_cast<unsigned>(digit - '0');
        for (auto &limb : limbs) {
            carry = accumulate(limb, carry);
        }
        if (carry != 0) {
            limbs.push_back(carry);
        }
    }
    return integer_words(limbs, false);
}
} // namespace erlang_aot::runtime::detail
