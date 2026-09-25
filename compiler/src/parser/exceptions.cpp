#include "forms.hpp"

namespace erlang_aot {
ast::CatchClause FormParser::catch_clause() {
    const auto begin = cursor_.offset();
    std::optional<std::variant<ast::Atom, ast::Variable>> exception_class;
    const auto *next = cursor_.peek(1);
    if (next && syntax(*next, U":")) {
        exception_class = atom_or_variable();
        expect(U":");
    }
    auto reason = pattern();
    std::optional<ast::Variable> stacktrace;
    if (exception_class && cursor_.take_syntax(U":")) {
        stacktrace = ast::Variable{value<std::u32string>(category(TokenKind::variable, "stacktrace variable"))};
    }
    auto guards = optional_guard();
    expect(U"->");
    auto body = sequence();
    return {.exception_class = std::move(exception_class),
            .reason = std::move(reason),
            .stacktrace = std::move(stacktrace),
            .guard = std::move(guards),
            .body = std::move(body),
            .source = builder_.source(begin, cursor_.offset(), begin)};
}

ast::TryExpression FormParser::try_expression() {
    ast::TryExpression result{.body = sequence(), .of = {}, .handlers = {}, .after = {}};
    if (cursor_.take_syntax(U"of")) {
        result.of = branches();
    }
    if (cursor_.take_syntax(U"catch")) {
        result.handlers.emplace();
        do {
            result.handlers->push_back(catch_clause());
        } while (cursor_.take_syntax(U";"));
    }
    if (cursor_.take_syntax(U"after")) {
        result.after = sequence();
    }
    if (!result.handlers && !result.after) {
        fail(DiagnosticCode::parser_syntax, "try requires catch or after");
    }
    expect(U"end");
    return result;
}

std::variant<ast::ExprId, ast::MaybeMatch> FormParser::maybe_item() {
    const auto begin = cursor_.offset();
    auto left = expression();
    const auto end = cursor_.offset();
    if (!cursor_.take_syntax(U"?=")) {
        return left;
    }
    node();
    auto candidate = builder_.pattern(ast::PatternCandidate{std::move(left)}, builder_.source(begin, end, begin));
    auto right = expression();
    return ast::MaybeMatch{.pattern = std::move(candidate),
                           .value = std::move(right),
                           .source = builder_.source(begin, cursor_.offset(), end)};
}

ast::MaybeExpression FormParser::maybe_expression() {
    ast::MaybeExpression result;
    do {
        result.body.push_back(maybe_item());
    } while (cursor_.take_syntax(U","));
    if (cursor_.take_syntax(U"else")) {
        result.otherwise = branches();
    }
    expect(U"end");
    return result;
}
} // namespace erlang_aot
