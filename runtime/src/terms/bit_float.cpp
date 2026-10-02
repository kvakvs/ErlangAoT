#include "bitstrings.hpp"
#include "floats.hpp"
#include "terms.hpp"
#include <bit>
#include <cmath>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
namespace {
// Round a nonnegative finite significand to nearest integer with ties to even.
std::uint32_t nearest(double value) {
    const auto lower = std::floor(value);
    const auto fraction = value - lower;
    return static_cast<std::uint32_t>(lower) +
           static_cast<unsigned>(fraction > 0.5 || (fraction == 0.5 && std::fmod(lower, 2.0) != 0));
}

// Encode binary16 directly from binary64, preserving signed zero, subnormals and rounded overflow.
std::uint16_t half(double value) {
    const auto sign = std::signbit(value) ? 0x8000U : 0U;
    const auto magnitude = std::abs(value);
    if (magnitude < std::ldexp(1.0, -14)) {
        return static_cast<std::uint16_t>(sign | nearest(std::ldexp(magnitude, 24)));
    }
    int exponent = 0;
    std::frexp(magnitude, &exponent);
    --exponent;
    if (exponent > 15) {
        return static_cast<std::uint16_t>(sign | 0x7c00U);
    }
    auto mantissa = nearest(std::ldexp(magnitude, 10 - exponent));
    if (mantissa == 2048) {
        ++exponent;
        mantissa = 1024;
    }
    return static_cast<std::uint16_t>(sign | (static_cast<unsigned>(exponent + 15) << 10) | (mantissa - 1024));
}

// Decode finite binary16 values exactly; exponent 31 rejects the enclosing pattern.
TermResult<double> unhalf(std::uint16_t bits) {
    const auto exponent = (bits >> 10) & 31;
    if (exponent == 31) {
        return std::unexpected(TermError::invalid_argument);
    }
    const auto fraction = bits & 1023;
    const auto value = exponent == 0 ? std::ldexp(static_cast<double>(fraction), -24)
                                     : std::ldexp(static_cast<double>(1024 + fraction), exponent - 25);
    return (bits & 0x8000) != 0 ? -value : value;
}
} // namespace

TermResult<Integer> bit_float_bits(const Term &value, std::size_t width) {
    const auto number = number_float(value);
    if (!number) {
        return std::unexpected(number.error());
    }
    switch (width) {
    case 16:
        return Integer{half(*number)};
    case 32:
        return Integer{std::bit_cast<std::uint32_t>(static_cast<float>(*number))};
    case 64:
        return Integer{std::bit_cast<std::uint64_t>(*number)};
    default:
        return std::unexpected(TermError::invalid_argument);
    }
}

TermResult<Term> bit_read_float(ProcessContext &context, const Integer &bits, std::size_t width) {
    TermFactory factory(context);
    switch (width) {
    case 0:
        return factory.floating(0.0);
    case 16:
        return unhalf(bits.convert_to<std::uint16_t>()).and_then([&](double value) { return factory.floating(value); });
    case 32:
        return factory.floating(std::bit_cast<float>(bits.convert_to<std::uint32_t>()));
    case 64:
        return factory.floating(std::bit_cast<double>(bits.convert_to<std::uint64_t>()));
    default:
        return std::unexpected(TermError::invalid_argument);
    }
}
} // namespace erlang_aot::runtime::detail
