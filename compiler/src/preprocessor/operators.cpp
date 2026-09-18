#include "expression.hpp"
#include <cmath>
#include <map>

namespace erlang_aot {
namespace {
// Erlang boolean operators require the atoms true/false, including non-short-circuit forms.
bool boolean_value(const Value &value) {
    if (value.kind != ValueKind::atom || (value.text != U"true" && value.text != U"false")) {
        throw EvaluationFailure();
    }
    return truth(value);
}

Value shift(const Value &left, const Value &right, bool forward) {
    BigInt amount = integral(right);
    if (amount < 0) {
        amount = -amount;
        forward = !forward;
    }
    if (amount > 1000000) {
        throw EvaluationLimit();
    }
    const auto count = amount.convert_to<unsigned>();
    const auto &number = integral(left);
    if (forward) {
        return integer(number << count);
    }
    return integer(number >> count);
}

// Update an owned integer to avoid temporary expression lifetimes and redundant allocations.
Value integer_arithmetic(std::u32string_view operation, BigInt result, const BigInt &right) {
    if (operation == U"+") {
        result += right;
    } else if (operation == U"-") {
        result -= right;
    } else {
        result *= right;
    }
    return integer(std::move(result));
}

Value arithmetic(std::u32string_view operation, const Value &a, const Value &b) {
    if (operation != U"/" && a.kind == ValueKind::integer && b.kind == ValueKind::integer) {
        return integer_arithmetic(operation, a.integer, b.integer);
    }
    const auto left = real(a);
    const auto right = real(b);
    if (operation == U"+") {
        return floating(left + right);
    }
    if (operation == U"-") {
        return floating(left - right);
    }
    if (operation == U"*") {
        return floating(left * right);
    }
    if (right == 0) {
        throw EvaluationFailure();
    }
    return floating(left / right);
}

Value integer_operation(std::u32string_view operation, const Value &a, const Value &b) {
    const auto &left = integral(a);
    const auto &right = integral(b);
    if (operation == U"band") {
        return integer(left & right);
    }
    if (operation == U"bor") {
        return integer(left | right);
    }
    if (operation == U"bxor") {
        return integer(left ^ right);
    }
    if (operation == U"bsl" || operation == U"bsr") {
        return shift(a, b, operation == U"bsl");
    }
    if (right == 0) {
        throw EvaluationFailure();
    }
    if (operation == U"div") {
        return integer(left / right);
    }
    return integer(left % right);
}

Value unary(std::u32string_view operation, const Value &value) {
    if (operation == U"not") {
        return boolean(!boolean_value(value));
    }
    if (operation == U"bnot") {
        return integer(~integral(value));
    }
    if (!numeric(value)) {
        throw EvaluationFailure();
    }
    if (operation == U"+") {
        return value;
    }
    return value.kind == ValueKind::integer ? integer(-value.integer) : floating(-value.real);
}

Value comparison(std::u32string_view operation, const Value &a, const Value &b) {
    const auto result = compare(a, b, operation == U"=:=" || operation == U"=/=");
    static const std::map<std::u32string_view, bool (*)(int)> operations{
        {U"==", [](int n) { return n == 0; }}, {U"=:=", [](int n) { return n == 0; }},
        {U"/=", [](int n) { return n != 0; }}, {U"=/=", [](int n) { return n != 0; }},
        {U"<", [](int n) { return n < 0; }},   {U"=<", [](int n) { return n <= 0; }},
        {U">", [](int n) { return n > 0; }},   {U">=", [](int n) { return n >= 0; }}};
    return boolean(operations.at(operation)(result));
}
} // namespace

bool operator_signature(std::u32string_view name, std::size_t arity) {
    static const std::set<std::u32string_view> unary_names{U"+", U"-", U"not", U"bnot"};
    static const std::set<std::u32string_view> binary_names{
        U"+",   U"-",  U"*",   U"/",  U"div", U"rem", U"band", U"bor", U"bxor", U"bsl", U"bsr",
        U"and", U"or", U"xor", U"==", U"/=",  U"=:=", U"=/=",  U"<",   U"=<",   U">",   U">="};
    return (arity == 1 && unary_names.contains(name)) || (arity == 2 && binary_names.contains(name));
}

Value evaluate_operator(std::u32string_view name, const std::vector<Value> &arguments) {
    if (arguments.size() == 1) {
        return unary(name, arguments.front());
    }
    const auto &a = arguments[0];
    const auto &b = arguments[1];
    static const std::set<std::u32string_view> arithmetic_names{U"+", U"-", U"*", U"/"};
    if (arithmetic_names.contains(name)) {
        return arithmetic(name, a, b);
    }
    static const std::set<std::u32string_view> integers{U"div", U"rem", U"band", U"bor", U"bxor", U"bsl", U"bsr"};
    if (integers.contains(name)) {
        return integer_operation(name, a, b);
    }
    if (name == U"and") {
        const bool x = boolean_value(a);
        return boolean(boolean_value(b) && x);
    }
    if (name == U"or") {
        const bool x = boolean_value(a);
        return boolean(boolean_value(b) || x);
    }
    if (name == U"xor") {
        return boolean(boolean_value(a) != boolean_value(b));
    }
    return comparison(name, a, b);
}
} // namespace erlang_aot
