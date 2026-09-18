#include "forms.hpp"

namespace erlang_aot {
// Wrap the selected grammar entry point without claiming semantic pattern validity.
ast::PatternSyntaxId FormParser::pattern(bool permissive) {
    const auto begin = cursor_.offset();
    auto child = expression(0, permissive ? OperatorContext::expression : OperatorContext::pattern);
    ast::PatternValue value = ast::RestrictedPattern{child};
    if (permissive)
        value = ast::PatternCandidate{std::move(child)};
    node();
    return builder_.pattern(std::move(value), builder_.source(begin, cursor_.offset(), begin));
}

// Parse restricted argument patterns, permitting the empty-arity head.
std::vector<ast::PatternSyntaxId> FormParser::arguments() {
    expect(U"(");
    std::vector<ast::PatternSyntaxId> result;
    if (cursor_.take_syntax(U")"))
        return result;
    do {
        result.push_back(pattern());
    } while (cursor_.take_syntax(U","));
    expect(U")");
    return result;
}

// Keep a nonempty comma sequence and leave enclosing delimiters unconsumed.
std::vector<ast::ExprId> FormParser::sequence() {
    std::vector<ast::ExprId> result;
    do {
        result.push_back(expression());
    } while (cursor_.take_syntax(U","));
    return result;
}

// Preserve semicolon alternatives and per-conjunction extents through the body arrow.
ast::GuardSyntax FormParser::guard(std::size_t begin) {
    std::vector<ast::GuardConjunction> alternatives;
    do {
        const auto start = cursor_.offset();
        auto tests = sequence();
        alternatives.push_back({std::move(tests), builder_.source(start, cursor_.offset(), start)});
    } while (cursor_.take_syntax(U";"));
    return {std::move(alternatives), builder_.source(begin, cursor_.offset(), begin)};
}

// Complete one head, optional guard and nonempty body before publishing a clause.
ast::FunctionClause FormParser::clause(std::size_t begin) {
    auto args = arguments();
    std::optional<ast::GuardSyntax> guards;
    const auto when = cursor_.offset();
    if (cursor_.take_syntax(U"when"))
        guards = guard(when);
    expect(U"->");
    auto body = sequence();
    return {std::move(args), std::move(guards), std::move(body), builder_.source(begin, cursor_.offset(), begin)};
}

// Check name/arity consistency within this form; separate definitions remain independent.
ast::Function FormParser::function() {
    const auto begin = cursor_.offset();
    ast::Atom name{value<std::u32string>(category(TokenKind::atom, "function name"))};
    std::vector<ast::FunctionClause> clauses;
    clauses.push_back(clause(begin));
    const auto arity = clauses.front().arguments.size();
    while (cursor_.take_syntax(U";")) {
        const auto start = cursor_.offset();
        const auto &next = category(TokenKind::atom, "function clause name");
        auto current = clause(start);
        if (value<std::u32string>(next) != name.name || current.arguments.size() != arity) {
            throw token_diagnostic(DiagnosticCode::parser_syntax, "function head mismatch", next);
        }
        clauses.push_back(std::move(current));
    }
    return {std::move(name), std::move(clauses)};
}
} // namespace erlang_aot
