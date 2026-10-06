#pragma once
#include "token_utils.hpp"
#include <boost/multiprecision/cpp_int.hpp>

namespace erlang_aot {
// Eager arithmetic keeps intermediate values owned, avoiding borrowed expression-template lifetimes.
using BigInt = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>, boost::multiprecision::et_off>;
enum class ValueKind : std::uint8_t {
    integer,
    floating,
    atom,
    reference,
    function,
    port,
    pid,
    tuple,
    map,
    nil,
    list,
    bits
};

struct Value {
    // Keep Erlang values independent of the future runtime's term layout.
    ValueKind kind = ValueKind::nil;
    BigInt integer = 0;
    double real = 0;
    std::u32string text;
    // Containers use ordered elements; maps alternate exact keys and values.
    std::vector<Value> elements;
    std::shared_ptr<Value> tail;
    // Store bitstrings exactly, including a final partial byte.
    std::vector<bool> bits;
};

// Serialize exact integers and decode finite binary64 independently of term structure.
std::string decimal_integer(const BigInt &number);
BigInt integer_from_double(double number);

// Largest integer magnitude in bits: the ERTS limit of 64-bit hosts (BIG_ARITY_MAX = 65,535 words of 64 bits), where
// OTP's scanner rejects larger literals; 32-bit targets allow 32 more bits at run time.
inline constexpr std::size_t INTEGER_BIT_LIMIT = 65'535 * 64;
// Parse canonical decimal digits with an optional leading '-'.
BigInt decimal_number(std::string_view decimal);
// Whether a canonical decimal integer has at most INTEGER_BIT_LIMIT magnitude bits; only literals as long as the
// largest one are parsed.
bool decimal_fits(std::string_view decimal);

// Construct and inspect primitive values without C++ truthiness/coercion leaks.
Value integer(BigInt number);
Value atom(std::u32string text);
Value boolean(bool truth);
Value floating(double number);
Value list(std::vector<Value> values);
bool truth(const Value &value);
bool numeric(const Value &value);
double real(const Value &value);
const BigInt &integral(const Value &value);
// Compare by Erlang term order, using exact numeric types for map keys/equality.
int compare(const Value &left, const Value &right, bool exact = false);
// Bound host allocations and convert validated nonnegative integer indices.
std::size_t index(const Value &value, std::size_t maximum = 1000000);
// Preserve the supplied diagnostic term without defining a public serialization.
std::vector<Token> term_tokens(const Value &value, const Token &site);
std::string display(const Value &value);

// Expression evaluation failures select false; syntax/guard violations are diagnostics.
class EvaluationFailure : public std::exception {};

// Distinguish implementation budgets from Erlang evaluation failures.
class EvaluationLimit : public std::exception {};
} // namespace erlang_aot
