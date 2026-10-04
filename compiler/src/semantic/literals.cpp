#include "binding_state.hpp"
#include "capabilities.hpp"
#include "records.hpp"
#include <charconv>
#include <erlang_aot/abi/term.hpp>
#include <limits>

namespace erlang_aot::semantic {
ast::ExprId ungroup(const ast::Module &syntax, ast::ExprId expression) {
    while (const auto *group = std::get_if<ast::Group>(&syntax.expression(expression).value)) {
        expression = group->expression;
    }
    return expression;
}

ast::ExprId pattern_root(const ast::Module &syntax, const ast::PatternSyntaxId &pattern) {
    return std::visit([](const auto &value) { return value.expression; }, syntax.pattern(pattern).value);
}

namespace {
// Share ABI v1 bounds with runtime/codegen while choosing the eventual target width explicitly.
bool fits_target(const std::int64_t number, const unsigned bits) {
    if (bits == 32) {
        return abi::v1::IntegerEncoding<32>::encode(number).has_value();
    }
    return bits == 64 && abi::v1::IntegerEncoding<64>::encode(number).has_value();
}

// Literal syntax can contain a character code or a canonical decimal integer.
std::optional<std::int64_t> literal_value(const ast::ExprValue &value) {
    if (const auto *character = std::get_if<ast::CharacterLiteral>(&value)) {
        return static_cast<std::int64_t>(character->value);
    }
    const auto *integer = std::get_if<ast::IntegerLiteral>(&value);
    if (!integer) {
        return {};
    }
    const auto &text = integer->value.decimal;
    std::int64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return {};
    }
    return result;
}

// Guard tests of every alternative, in source order.
void append_guard(const ast::GuardSyntax *guard, std::vector<ast::ExprId> &result) {
    if (guard) {
        for (const auto &alternative : guard->alternatives) {
            result.insert(result.end(), alternative.tests.begin(), alternative.tests.end());
        }
    }
}

// A case reads its scrutinee; every branch then reads each clause's guard tests and body.
// Patterns stay with match plans.
std::vector<ast::ExprId> branch_children(const ast::ExprValue &value, const std::vector<Branch> &clauses) {
    std::vector<ast::ExprId> result;
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        result.push_back(selection->value);
    }
    for (const auto &clause : clauses) {
        append_guard(clause.guard, result);
        result.insert(result.end(), clause.body->begin(), clause.body->end());
    }
    return result;
}
} // namespace

std::vector<Branch> branch_clauses(const ast::ExprValue &value) {
    std::vector<Branch> result;
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        for (const auto &clause : selection->clauses) {
            result.push_back({&clause.pattern, clause.guard ? &*clause.guard : nullptr, &clause.body});
        }
    } else if (const auto *choice = std::get_if<ast::IfExpression>(&value)) {
        for (const auto &clause : choice->clauses) {
            result.push_back({nullptr, &clause.guard, &clause.body});
        }
    }
    return result;
}

std::optional<std::int64_t> integer_literal(const ast::Module &syntax, ast::ExprId expression,
                                            const unsigned word_bits) {
    if (word_bits != 32 && word_bits != 64) {
        return {};
    }
    expression = ungroup(syntax, expression);
    const auto &value = syntax.expression(expression).value;
    const auto *unary = std::get_if<ast::UnaryExpression>(&value);
    auto number = literal_value(value);
    if (unary && unary->operation == ast::UnaryOperator::negative) {
        number = literal_value(syntax.expression(ungroup(syntax, unary->operand)).value);
        if (number && *number != std::numeric_limits<std::int64_t>::min()) {
            number = -*number;
        } else {
            return {};
        }
    }
    if (!number || !fits_target(*number, word_bits)) {
        return {};
    }
    return number;
}

std::vector<ast::ExprId> expression_children(const ast::Expression &expression) {
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        return call->arguments;
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        return {match->right};
    }
    if (const auto clauses = branch_clauses(expression.value); !clauses.empty()) {
        return branch_children(expression.value, clauses);
    }
    return binding_children(expression.value);
}

std::vector<ast::ExprId> expression_children(const Module &module, const ast::Expression &expression) {
    const auto *record = std::get_if<ast::RecordExpression>(&expression.value);
    if (!record || record->base) {
        return expression_children(expression);
    }
    std::vector<ast::ExprId> result;
    for (const auto &field : record_values(module, *record, false)) {
        if (field) {
            result.push_back(*field);
        }
    }
    return result;
}

std::vector<ast::ExprId> function_roots(const ast::Function &function) {
    std::vector<ast::ExprId> result;
    for (const auto &clause : function.clauses) {
        append_guard(clause.guard ? &*clause.guard : nullptr, result);
        result.insert(result.end(), clause.body.begin(), clause.body.end());
    }
    return result;
}
} // namespace erlang_aot::semantic
