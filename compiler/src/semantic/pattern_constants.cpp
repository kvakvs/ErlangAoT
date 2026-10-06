#include "../parsing/operator_info.hpp"
#include "../preprocessor/expression.hpp"
#include "pattern_state.hpp"
#include <set>

namespace erlang_aot::semantic {
namespace {
// Canonical decimal digits must not pass through Boost's octal/prefix parser; the lexer admitted only literals within
// the integer size limit.
BigInt decimal(const std::string &text) { return decimal_number(text); }

// Only arithmetic operators belong to constant patterns; guard booleans/comparisons do not.
std::u32string_view arithmetic(const ast::ExprValue &value) {
    if (const auto *unary = std::get_if<ast::UnaryExpression>(&value)) {
        return unary->operation == ast::UnaryOperator::logical_not ? U"" : operator_spelling(unary->operation);
    }
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    if (!binary) {
        return U"";
    }
    const auto name = operator_spelling(binary->operation);
    static const std::set<std::u32string_view> names{U"+",    U"-",   U"*",    U"/",   U"div", U"rem",
                                                     U"band", U"bor", U"bxor", U"bsl", U"bsr"};
    return names.contains(name) ? name : U"";
}

// Decode scalar leaves only; aggregate constants cannot produce a legal arithmetic result.
Value scalar(const ast::ExprValue &value) {
    if (const auto *number = std::get_if<ast::IntegerLiteral>(&value)) {
        return integer(decimal(number->value.decimal));
    }
    if (const auto *number = std::get_if<ast::FloatLiteral>(&value)) {
        return floating(number->value);
    }
    if (const auto *character = std::get_if<ast::CharacterLiteral>(&value)) {
        return integer(character->value);
    }
    if (const auto *name = std::get_if<ast::Atom>(&value)) {
        return atom(name->name);
    }
    throw EvaluationFailure();
}

struct Visit {
    // Postorder tasks avoid C++ recursion even for deeply nested arithmetic.
    ast::ExprId id;
    bool finish = false;
};

// Evaluate a completed arithmetic node using owned scalar operands.
void reduce(const ast::ExprValue &value, std::vector<Value> &values) {
    const auto count = std::holds_alternative<ast::UnaryExpression>(value) ? 1U : 2U;
    std::vector<Value> arguments(values.end() - count, values.end());
    values.erase(values.end() - count, values.end());
    // integer() refuses every intermediate past the size limit with EvaluationLimit.
    values.push_back(evaluate_operator(arithmetic(value), arguments));
}

// Schedule arithmetic only after rejecting nonconstant and nonarithmetic syntax.
void enter(const ast::ExprId &id, const ast::ExprValue &value, std::vector<Visit> &pending,
           std::vector<Value> &values) {
    if (const auto *group = std::get_if<ast::Group>(&value)) {
        pending.push_back({group->expression});
    } else if (!arithmetic(value).empty()) {
        pending.push_back({id, true});
        const auto children = binding_children(value);
        for (auto child = children.rbegin(); child != children.rend(); ++child) {
            pending.push_back({*child});
        }
    } else {
        values.push_back(scalar(value));
    }
}

// A scalar result has an explicit numeric type, preserving integer/float exact-match distinctions.
PatternLiteral result_literal(const Value &value) {
    if (value.kind == ValueKind::integer) {
        return ast::IntegerLiteral{Integer{decimal_integer(value.integer)}};
    }
    if (value.kind == ValueKind::floating) {
        return ast::FloatLiteral{value.real};
    }
    return ast::Atom{value.text};
}

// Charge scalar storage/operand work in addition to AST visits to bound arithmetic-heavy modules.
bool scalar_budget(BindingAnalysis &state, const ast::ExprId &id, const std::vector<Value> &values) {
    if (values.empty()) {
        return true;
    }
    const auto &value = values.back();
    // About the decimal digits of an integer (log10 2 = 0.30103), without converting a large one to text.
    const auto bits = value.kind == ValueKind::integer && value.integer != 0
                          ? boost::multiprecision::msb(boost::multiprecision::abs(value.integer)) + 1
                          : 0;
    const auto cost = value.kind == ValueKind::integer ? bits * 30'103 / 100'000 + 1 : 1;
    return state.spend(id, cost);
}

// A single value stack owns all intermediates and is discarded on any failed budget check.
std::optional<PatternLiteral> evaluate_constant(BindingAnalysis &state, const ast::ExprId &root) {
    std::vector<Visit> pending{{root}};
    std::vector<Value> values;
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id)) {
            return {};
        }
        const auto &value = state.module.syntax->expression(visit.id).value;
        if (visit.finish) {
            reduce(value, values);
        } else {
            enter(visit.id, value, pending, values);
        }
        if (!scalar_budget(state, visit.id, values)) {
            return {};
        }
    }
    return result_literal(values.back());
}
} // namespace

void pattern_error(BindingAnalysis &state, const ast::ExprId &id, std::string message) {
    state.invalid_pattern = true;
    report(state.module, &state.module.syntax->expression(id).source, std::move(message), state.out);
}

std::optional<PatternLiteral> pattern_constant(BindingAnalysis &state, const ast::ExprId &root) {
    try {
        return evaluate_constant(state, root);
    } catch (const EvaluationLimit &) {
        // OTP's linter reports a constant past the integer size limit as an illegal pattern too.
        pattern_error(state, root);
    } catch (const EvaluationFailure &) {
        pattern_error(state, root);
    }
    return {};
}
} // namespace erlang_aot::semantic
