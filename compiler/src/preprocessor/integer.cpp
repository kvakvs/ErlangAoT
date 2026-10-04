#include "value.hpp"
#include <cmath>

namespace erlang_aot {
// Keep decimal serialization independent of the surrounding recursive term structure.
std::string decimal_integer(const BigInt &number) { return number.str(); }

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

} // namespace erlang_aot
