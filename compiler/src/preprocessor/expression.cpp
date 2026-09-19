#include "expression.hpp"
#include <algorithm>

namespace erlang_aot {

// Literal token conversion shares scanner precision and decoded Unicode semantics.
Value literal_value(const Token &token) {
    if (const auto *number = std::get_if<Integer>(&token.value)) {
        return integer(BigInt(number->decimal));
    }
    if (const auto *number = std::get_if<double>(&token.value)) {
        return floating(*number);
    }
    if (token.kind == TokenKind::atom) {
        return atom(std::u32string(token.text()));
    }
    if (token.kind == TokenKind::string) {
        std::vector<Value> chars;
        for (const auto character : token.text()) {
            chars.push_back(integer(static_cast<std::uint32_t>(character)));
        }
        return list(std::move(chars));
    }
    pp_fail(DiagnosticCode::invalid_condition, "expected literal expression", token);
}

namespace {
// The pseudo-function takes a macro name, never an evaluated expression.
void validate_defined(const Expr &expression) {
    if (expression.children.size() != 1) {
        pp_fail(DiagnosticCode::invalid_condition, "defined expects one macro name", expression.token);
    }
    const auto &name = expression.children.front();
    if (name.kind == ExprKind::variable || (name.kind == ExprKind::literal && name.token.kind == TokenKind::atom)) {
        return;
    }
    pp_fail(DiagnosticCode::invalid_condition, "defined expects a macro name", name.token);
}

// Validate defined(Name) before any short-circuit evaluation takes place.
void validate_call(const Expr &expression) {
    if (expression.token.text() == U"defined" && expression.modifiers.empty()) {
        validate_defined(expression);
        return;
    }
    if (!expression.modifiers.empty() && operator_signature(expression.token.text(), expression.children.size())) {
        return;
    }
    if (!guard_signature(expression.token.text(), expression.children.size())) {
        pp_fail(DiagnosticCode::invalid_condition, "call is not an Erlang guard BIF", expression.token);
    }
}

Value binary(const Expr &expression, const std::function<bool(std::u32string_view)> &defined) {
    auto left = evaluate(expression.children[0], defined);
    const auto operation = expression.token.text();
    if (operation == U"andalso" || operation == U"orelse") {
        if (left.kind != ValueKind::atom || (left.text != U"true" && left.text != U"false")) {
            throw EvaluationFailure();
        }
        if (truth(left) == (operation == U"orelse")) {
            return left;
        }
        return evaluate(expression.children[1], defined);
    }
    return evaluate_operator(operation, {left, evaluate(expression.children[1], defined)});
}

// Normalize list tails so type tests and length distinguish improper lists correctly.
Value list_value(std::vector<Value> elements, bool has_tail) {
    if (!has_tail) {
        return list(std::move(elements));
    }
    auto last = elements.back();
    elements.pop_back();
    auto result = list(std::move(elements));
    if (last.kind == ValueKind::list || last.kind == ValueKind::nil) {
        result.elements.insert(result.elements.end(), last.elements.begin(), last.elements.end());
        result.tail = last.tail;
    } else {
        result.tail = std::make_shared<Value>(std::move(last));
    }
    return result;
}

// Apply exact map updates and associations in source order, replacing duplicate keys.
Value map_value(const Expr &expression, std::vector<Value> elements) {
    Value result;
    result.kind = ValueKind::map;
    std::size_t start = 0;
    if (expression.kind == ExprKind::map_update) {
        result = elements.front();
        start = 1;
    }
    if (result.kind != ValueKind::map) {
        throw EvaluationFailure();
    }
    for (std::size_t i = start; i < elements.size(); i += 2) {
        std::size_t found = 0;
        while (found < result.elements.size() && compare(result.elements[found], elements[i], true) != 0) {
            found += 2;
        }
        if (found < result.elements.size()) {
            result.elements[found + 1] = elements[i + 1];
            continue;
        }
        if (syntax(expression.modifiers[(i - start) / 2], U":=")) {
            throw EvaluationFailure();
        }
        result.elements.push_back(elements[i]);
        result.elements.push_back(elements[i + 1]);
    }
    return result;
}
} // namespace

void validate_guard(const Expr &expression) {
    if (expression.kind == ExprKind::call) {
        validate_call(expression);
    }
    if (expression.kind == ExprKind::literal) {
        literal_value(expression.token);
    }
    if (expression.kind == ExprKind::record && !expression.modifiers.empty()) {
        pp_fail(DiagnosticCode::invalid_condition, "record construction requires record expansion", expression.token);
    }
    if (expression.kind == ExprKind::external_fun) {
        pp_fail(DiagnosticCode::invalid_condition, "function construction is not a guard expression", expression.token);
    }
    if (syntax(expression.token, U"++") || syntax(expression.token, U"--")) {
        pp_fail(DiagnosticCode::invalid_condition, "list operators are not permitted in guards", expression.token);
    }
    for (const auto &child : expression.children) {
        validate_guard(child);
    }
}

bool literal_term(const Expr &expression) {
    if (expression.kind == ExprKind::literal || expression.kind == ExprKind::external_fun) {
        return true;
    }
    if (expression.kind == ExprKind::unary) {
        return (syntax(expression.token, U"+") || syntax(expression.token, U"-")) &&
               expression.children[0].kind == ExprKind::literal;
    }
    constexpr ExprKind containers[]{ExprKind::tuple, ExprKind::list, ExprKind::map, ExprKind::bits, ExprKind::segment};
    if (std::ranges::find(containers, expression.kind) == std::end(containers)) {
        return false;
    }
    return std::ranges::all_of(expression.children, literal_term);
}

namespace {
// Evaluate collection members and guard arguments in source order.
Value evaluate_collection(const Expr &expression, const std::function<bool(std::u32string_view)> &defined) {
    std::vector<Value> values;
    values.reserve(expression.children.size());
    for (const auto &child : expression.children) {
        values.push_back(evaluate(child, defined));
    }
    switch (expression.kind) {
    case ExprKind::call:
        return operator_signature(expression.token.text(), values.size())
                   ? evaluate_operator(expression.token.text(), values)
                   : guard_call(expression.token.text(), values);
    case ExprKind::external_fun: {
        index(values[1], 255);
        Value result;
        result.kind = ValueKind::function;
        result.text = std::u32string(expression.token.text());
        result.elements = std::move(values);
        return result;
    }
    case ExprKind::unary:
        return evaluate_operator(expression.token.text(), values);
    case ExprKind::list:
        return list_value(std::move(values), !expression.modifiers.empty());
    case ExprKind::map:
    case ExprKind::map_update:
        return map_value(expression, std::move(values));
    default: {
        Value result;
        result.kind = ValueKind::tuple;
        result.elements = std::move(values);
        return result;
    }
    }
}
} // namespace

Value evaluate(const Expr &expression, const std::function<bool(std::u32string_view)> &defined) {
    if (expression.kind == ExprKind::literal) {
        return literal_value(expression.token);
    }
    if (expression.kind == ExprKind::variable || expression.kind == ExprKind::record) {
        throw EvaluationFailure();
    }
    if (expression.kind == ExprKind::binary) {
        return binary(expression, defined);
    }
    if (expression.kind == ExprKind::bits) {
        return evaluate_bits(expression, defined);
    }
    if (expression.kind == ExprKind::call && expression.token.text() == U"defined") {
        return boolean(defined(expression.children[0].token.text()));
    }
    return evaluate_collection(expression, defined);
}

bool condition(std::span<const Token> input, std::size_t depth,
               const std::function<bool(std::u32string_view)> &defined) {
    auto expression = ExpressionParser(input, depth).parse();
    try {
        validate_guard(expression);
        return truth(evaluate(expression, defined));
    } catch (const EvaluationLimit &) {
        pp_fail(DiagnosticCode::resource_limit, "condition value budget exhausted", expression.token);
    } catch (const EvaluationFailure &) {
        return false;
    }
}

Value parse_term(std::span<const Token> input, std::size_t depth) {
    const auto expression = ExpressionParser(input, depth).parse();
    if (!literal_term(expression)) {
        pp_fail(DiagnosticCode::malformed_directive, "expected literal Erlang term", expression.token);
    }
    try {
        return evaluate(expression, [](std::u32string_view) { return false; });
    } catch (const EvaluationLimit &) {
        pp_fail(DiagnosticCode::resource_limit, "term value budget exhausted", expression.token);
    } catch (const EvaluationFailure &) {
        pp_fail(DiagnosticCode::malformed_directive, "invalid Erlang term", expression.token);
    }
}
} // namespace erlang_aot
