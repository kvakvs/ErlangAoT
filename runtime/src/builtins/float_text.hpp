#pragma once
#include <optional>
#include <string>

namespace erlang_aot::runtime::builtins {
// One float_to_list/2 format: OTP's default and {scientific, D} ("%.*e"), {decimals, D} (fixed, `compact` trims
// trailing zeros) and short (shortest round-trip digits).
struct FloatFormat {
    enum class Kind : unsigned char { scientific, fixed, shortest };
    Kind kind = Kind::scientific;
    // Digits after the decimal point; OTP's default is 20.
    long long decimals = 20;
    bool compact = false;
};

// The text OTP's float_to_list(Value, Options) produces; none where OTP raises badarg (negative decimals, or
// text that would not fit OTP's 256-byte buffer).
std::optional<std::string> float_text(double value, const FloatFormat &format);
} // namespace erlang_aot::runtime::builtins
