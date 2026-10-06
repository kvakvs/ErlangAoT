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

// A case reads its scrutinee and a try or maybe its body first; every branch then reads each clause's guard tests
// and body, and a try's after body comes last. Patterns stay with match plans.
std::vector<ast::ExprId> branch_children(const ast::ExprValue &value, const std::vector<Branch> &clauses) {
    std::vector<ast::ExprId> result;
    const auto *attempt = std::get_if<ast::TryExpression>(&value);
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        result.push_back(selection->value);
    } else if (attempt) {
        result = attempt->body;
    } else if (const auto *conditional = std::get_if<ast::MaybeExpression>(&value)) {
        result = maybe_operands(*conditional);
    }
    for (const auto &clause : clauses) {
        append_guard(clause.guard, result);
        result.insert(result.end(), clause.body->begin(), clause.body->end());
    }
    if (attempt && attempt->after) {
        result.insert(result.end(), attempt->after->begin(), attempt->after->end());
    }
    return result;
}

// Present branch clauses (case, or a try's of part) uniformly.
void append_branches(const std::vector<ast::BranchClause> &clauses, std::vector<Branch> &result) {
    for (const auto &clause : clauses) {
        result.push_back({&clause.pattern, clause.guard ? &*clause.guard : nullptr, &clause.body});
    }
}

// A try lists its of clauses before its catch clauses.
void append_try(const ast::TryExpression &attempt, std::vector<Branch> &result) {
    if (attempt.of) {
        append_branches(*attempt.of, result);
    }
    if (attempt.handlers) {
        for (const auto &clause : *attempt.handlers) {
            result.push_back({&clause.reason, clause.guard ? &*clause.guard : nullptr, &clause.body, &clause});
        }
    }
}
} // namespace

std::vector<Branch> branch_clauses(const ast::ExprValue &value) {
    std::vector<Branch> result;
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        append_branches(selection->clauses, result);
    } else if (const auto *choice = std::get_if<ast::IfExpression>(&value)) {
        for (const auto &clause : choice->clauses) {
            result.push_back({nullptr, &clause.guard, &clause.body});
        }
    } else if (const auto *attempt = std::get_if<ast::TryExpression>(&value)) {
        append_try(*attempt, result);
    } else if (const auto *conditional = std::get_if<ast::MaybeExpression>(&value);
               conditional && conditional->otherwise) {
        append_branches(*conditional->otherwise, result);
    }
    return result;
}

std::vector<ast::ExprId> maybe_operands(const ast::MaybeExpression &value) {
    std::vector<ast::ExprId> result;
    for (const auto &item : value.body) {
        const auto *match = std::get_if<ast::MaybeMatch>(&item);
        result.push_back(match ? match->value : std::get<ast::ExprId>(item));
    }
    return result;
}

std::size_t first_handler(const ast::ExprValue &value) {
    if (const auto *attempt = std::get_if<ast::TryExpression>(&value)) {
        return attempt->of ? attempt->of->size() : 0;
    }
    if (std::holds_alternative<ast::MaybeExpression>(value)) {
        return 0;
    }
    return branch_clauses(value).size();
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
    if (const auto clauses = branch_clauses(expression.value);
        !clauses.empty() || std::holds_alternative<ast::TryExpression>(expression.value) ||
        std::holds_alternative<ast::MaybeExpression>(expression.value)) {
        return branch_children(expression.value, clauses);
    }
    if (comprehension_qualifiers(expression.value)) {
        return comprehension_children(expression.value);
    }
    return binding_children(expression.value);
}

std::vector<ast::ExprId> expression_children(const Module &module, const ast::Expression &expression) {
    const auto *record = std::get_if<ast::RecordExpression>(&expression.value);
    if (!record) {
        return expression_children(expression);
    }
    std::vector<ast::ExprId> result;
    if (record->base) {
        // OTP evaluates the update values in source order before the updated record.
        for (const auto &field : record->fields) {
            result.push_back(field.value);
        }
        result.push_back(*record->base);
        return result;
    }
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
