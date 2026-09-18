#pragma once
#include <erlang_aot/compiler/ast/operators.hpp>
#include <stdexcept>
#include <string_view>

// Test-only spellings keep structural records independent of production operator lookup.
inline std::string_view spelling(erlang_aot::ast::BinaryOperator operation) {
    using enum erlang_aot::ast::BinaryOperator;
    switch (operation) {
    case send:
        return "!";
    case or_else:
        return "orelse";
    case and_also:
        return "andalso";
    case equal:
        return "==";
    case not_equal:
        return "/=";
    case less_equal:
        return "=<";
    case less:
        return "<";
    case greater_equal:
        return ">=";
    case greater:
        return ">";
    case exact_equal:
        return "=:=";
    case exact_not_equal:
        return "=/=";
    case append:
        return "++";
    case subtract_list:
        return "--";
    case add:
        return "+";
    case subtract:
        return "-";
    case bit_or:
        return "bor";
    case bit_xor:
        return "bxor";
    case shift_left:
        return "bsl";
    case shift_right:
        return "bsr";
    case logical_or:
        return "or";
    case logical_xor:
        return "xor";
    case multiply:
        return "*";
    case divide:
        return "/";
    case integer_divide:
        return "div";
    case remainder:
        return "rem";
    case bit_and:
        return "band";
    case logical_and:
        return "and";
    }
    throw std::runtime_error("unmapped binary operator");
}

inline std::string_view spelling(erlang_aot::ast::UnaryOperator operation) {
    using enum erlang_aot::ast::UnaryOperator;
    switch (operation) {
    case positive:
        return "+";
    case negative:
        return "-";
    case bit_not:
        return "bnot";
    case logical_not:
        return "not";
    }
    throw std::runtime_error("unmapped unary operator");
}
