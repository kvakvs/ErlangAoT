#include "float_text.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>

// float_to_list/1,2 text, reproducing OTP's formatting (erts sys_double_to_chars_ext/_fast and its Ryu shortest
// output) from its documented behavior.
namespace erlang_aot::runtime::builtins {
namespace {
// OTP formats into a 256-byte buffer: text of 256 bytes or more is badarg.
constexpr std::size_t BUFFER_SIZE = 256;
// Below 19 decimals and up to 2^53 OTP formats fixed text itself; larger values go through printf.
constexpr int FAST_DECIMALS = 19;
constexpr double FAST_LIMIT = 9007199254740992.0;

// printf `format` with `precision` (a negative precision is the default 6, as in C); none past the buffer.
std::optional<std::string> printed(double value, const char *format, long long precision) {
    if (precision >= static_cast<long long>(BUFFER_SIZE)) {
        return std::nullopt;
    }
    std::array<char, BUFFER_SIZE> buffer{};
    const auto digits = precision < 0 ? -1 : static_cast<int>(precision);
    const auto length = std::snprintf(buffer.data(), buffer.size(), format, digits, value);
    if (length < 0 || static_cast<std::size_t>(length) >= buffer.size()) {
        return std::nullopt;
    }
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

// Drop trailing zeros, keeping one digit after a decimal point (OTP's find_first_trailing_zero, which also trims an
// integer's zeros).
void trim_zeros(std::string &text) {
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
}

// Fixed text below 2^53 with fewer than 19 decimals: the fraction rounds half away from zero on its own.
std::string fast_fixed(double value, const FloatFormat &format) {
    const auto decimals = static_cast<int>(format.decimals);
    static constexpr std::array<double, FAST_DECIMALS> POWERS{1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8, 1e9,
                                                              1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18};
    const double magnitude = std::fabs(value);
    std::string text = std::signbit(value) ? "-" : "";
    if (decimals == 0) {
        return text + std::to_string(static_cast<std::uint64_t>(std::llround(magnitude)));
    }
    const double whole = std::floor(magnitude);
    const double scaled = std::round((magnitude - whole) * POWERS.at(static_cast<std::size_t>(decimals)));
    auto integer = static_cast<std::uint64_t>(whole);
    auto fraction = static_cast<std::uint64_t>(scaled);
    if (scaled >= POWERS.at(static_cast<std::size_t>(decimals))) {
        ++integer;
        fraction = 0;
    }
    const auto digits = std::to_string(fraction);
    return text + std::to_string(integer) + "." + std::string(static_cast<std::size_t>(decimals) - digits.size(), '0') +
           digits;
}

// {decimals, D}: fixed notation; `compact` trims trailing zeros.
std::optional<std::string> fixed(double value, long long decimals, bool compact) {
    if (decimals < 0) {
        return std::nullopt;
    }
    if (std::fabs(value) > FAST_LIMIT || decimals >= FAST_DECIMALS) {
        auto text = printed(value, "%.*f", decimals);
        if (text && compact) {
            trim_zeros(*text);
        }
        return text;
    }
    auto text = fast_fixed(value, {.kind = FloatFormat::Kind::fixed, .decimals = decimals});
    if (compact && decimals > 0) {
        trim_zeros(text);
    }
    return text;
}

// Shortest round-trip digits and the decimal exponent of the first one.
struct Shortest {
    std::string digits;
    int exponent = 0;
};

// The shortest digits of a nonzero finite magnitude, from scientific to_chars ("d.ddde+XX").
Shortest shortest_digits(double magnitude) {
    std::array<char, 32> buffer{};
    const auto end =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific).ptr;
    const std::string_view text(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
    const auto mark = text.find('e');
    Shortest result;
    for (const char digit : text.substr(0, mark)) {
        if (digit != '.') {
            result.digits.push_back(digit);
        }
    }
    std::from_chars(text.data() + mark + (text[mark + 1] == '+' ? 2 : 1), text.data() + text.size(), result.exponent);
    return result;
}

// Whether digits times 10^exponent is an integer from 2^53 on, which OTP prints in scientific notation.
bool wide_integer(const std::string &digits, int exponent) {
    const auto output = std::stoull(digits);
    return (exponent == 0 && output >= (1ULL << 53)) || (exponent == 1 && output > (1ULL << 52) / 5) ||
           (exponent == 2 && output > (1ULL << 51) / 25);
}

// Whether OTP prints shortest digits in fixed notation: exponents in a window, except integers from 2^53 on.
bool fixed_notation(const Shortest &shortest) {
    const auto length = static_cast<int>(shortest.digits.size());
    const int exponent = shortest.exponent - (length - 1);
    const int lower = length == 1 ? -4 : -(length + 2);
    const int upper = length == 1 || shortest.exponent >= 10 ? 2 : 1;
    return exponent >= lower && exponent <= upper && !wide_integer(shortest.digits, exponent);
}

// Fixed notation of shortest digits: "1729.0", "17.29", "0.001729".
std::string fixed_digits(const Shortest &shortest) {
    const auto length = static_cast<int>(shortest.digits.size());
    const int whole = shortest.exponent + 1;
    if (whole >= length) {
        return shortest.digits + std::string(static_cast<std::size_t>(whole - length), '0') + ".0";
    }
    if (whole > 0) {
        return shortest.digits.substr(0, static_cast<std::size_t>(whole)) + "." +
               shortest.digits.substr(static_cast<std::size_t>(whole));
    }
    return "0." + std::string(static_cast<std::size_t>(-whole), '0') + shortest.digits;
}

// short: the shortest round-trip digits, in fixed or scientific notation ("1.0e10", "1.234e-5").
std::string shortest(double value) {
    const std::string sign = std::signbit(value) ? "-" : "";
    if (value == 0) {
        return sign + "0.0";
    }
    const auto digits = shortest_digits(std::fabs(value));
    if (fixed_notation(digits)) {
        return sign + fixed_digits(digits);
    }
    const auto fraction = digits.digits.size() > 1 ? digits.digits.substr(1) : "0";
    return sign + digits.digits.substr(0, 1) + "." + fraction + "e" + std::to_string(digits.exponent);
}
} // namespace

std::optional<std::string> float_text(double value, const FloatFormat &format) {
    switch (format.kind) {
    case FloatFormat::Kind::fixed:
        return fixed(value, format.decimals, format.compact);
    case FloatFormat::Kind::shortest:
        return shortest(value);
    default:
        return printed(value, "%.*e", format.decimals);
    }
}
} // namespace erlang_aot::runtime::builtins
