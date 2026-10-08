#include "value.hpp"
#include <cmath>

namespace clause {
// Keep decimal serialization independent of the surrounding recursive term structure.
std::string decimal_integer(const BigInt &number) { return number.str(); }

namespace {
// Decimal digits of the largest magnitude, 2^INTEGER_BIT_LIMIT - 1.
constexpr std::size_t INTEGER_DIGIT_LIMIT = 1'262'593;
// Digits added per multiplication while parsing: 10^19 fits in 64 bits.
constexpr std::size_t CHUNK_DIGITS = 19;
} // namespace

BigInt decimal_number(std::string_view decimal) {
    const bool negative = decimal.starts_with('-');
    if (negative) {
        decimal.remove_prefix(1);
    }
    BigInt result = 0;
    for (std::size_t at = 0; at < decimal.size(); at += CHUNK_DIGITS) {
        std::uint64_t scale = 1;
        std::uint64_t chunk = 0;
        for (const char digit : decimal.substr(at, CHUNK_DIGITS)) {
            scale *= 10;
            chunk = chunk * 10 + static_cast<unsigned>(digit - '0');
        }
        result = result * scale + chunk;
    }
    return negative ? BigInt(-result) : result;
}

bool decimal_fits(std::string_view decimal) {
    const auto digits = decimal.size() - (decimal.starts_with('-') ? 1 : 0);
    if (digits != INTEGER_DIGIT_LIMIT) {
        return digits < INTEGER_DIGIT_LIMIT;
    }
    const auto magnitude = boost::multiprecision::abs(decimal_number(decimal));
    return boost::multiprecision::msb(magnitude) < INTEGER_BIT_LIMIT;
}

// Convert finite binary64 through its exact mantissa; avoid wide floating-to-integer casts.
BigInt integer_from_double(const double number) {
    if (!std::isfinite(number)) {
        throw std::invalid_argument("nonfinite integer");
    }
    if (std::abs(number) < 1) {
        return 0;
    }
    int exponent = 0;
    const double fraction = std::frexp(std::abs(number), &exponent);
    const auto mantissa = static_cast<std::uint64_t>(std::ldexp(fraction, 53));
    BigInt result = exponent <= 53 ? BigInt(mantissa >> (53 - exponent)) : BigInt(mantissa);
    if (exponent > 53) {
        result <<= static_cast<unsigned>(exponent - 53);
    }
    return number < 0 ? -result : result;
}

} // namespace clause
