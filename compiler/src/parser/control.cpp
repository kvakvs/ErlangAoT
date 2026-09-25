#include "forms.hpp"

namespace erlang_aot {
std::optional<ast::ExprValue> FormParser::control(const OperatorContext context) {
    if (context == OperatorContext::pattern) {
        return std::nullopt;
    }
    if (cursor_.take_syntax(U"begin")) {
        return block();
    }
    if (cursor_.take_syntax(U"case")) {
        return case_expression();
    }
    if (cursor_.take_syntax(U"if")) {
        return if_expression();
    }
    if (cursor_.take_syntax(U"receive")) {
        return receive_expression();
    }
    if (cursor_.take_syntax(U"fun")) {
        return fun_expression();
    }
    if (cursor_.take_syntax(U"try")) {
        return try_expression();
    }
    if (cursor_.take_syntax(U"maybe")) {
        return maybe_expression();
    }
    return std::nullopt;
}

ast::BlockExpression FormParser::block() {
    auto body = sequence();
    expect(U"end");
    return {std::move(body)};
}

ast::BranchClause FormParser::branch() {
    const auto begin = cursor_.offset();
    auto candidate = pattern(true);
    auto guards = optional_guard();
    expect(U"->");
    auto body = sequence();
    return {.pattern = std::move(candidate),
            .guard = std::move(guards),
            .body = std::move(body),
            .source = builder_.source(begin, cursor_.offset(), begin)};
}

std::vector<ast::BranchClause> FormParser::branches() {
    std::vector<ast::BranchClause> result;
    do {
        result.push_back(branch());
    } while (cursor_.take_syntax(U";"));
    return result;
}

ast::CaseExpression FormParser::case_expression() {
    auto value = expression();
    expect(U"of");
    auto clauses = branches();
    expect(U"end");
    return {.value = std::move(value), .clauses = std::move(clauses)};
}

ast::IfExpression FormParser::if_expression() {
    std::vector<ast::IfClause> clauses;
    do {
        const auto begin = cursor_.offset();
        auto guards = guard(begin);
        expect(U"->");
        auto body = sequence();
        clauses.push_back({.guard = std::move(guards),
                           .body = std::move(body),
                           .source = builder_.source(begin, cursor_.offset(), begin)});
    } while (cursor_.take_syntax(U";"));
    expect(U"end");
    return {std::move(clauses)};
}

ast::ReceiveExpression FormParser::receive_expression() {
    ast::ReceiveExpression result;
    if (!syntax(cursor_.anchor(), U"after")) {
        result.clauses = branches();
    }
    const auto begin = cursor_.offset();
    if (cursor_.take_syntax(U"after")) {
        auto timeout = expression();
        expect(U"->");
        auto body = sequence();
        result.after = ast::ReceiveTimeout{.timeout = std::move(timeout),
                                           .body = std::move(body),
                                           .source = builder_.source(begin, cursor_.offset(), begin)};
    }
    expect(U"end");
    return result;
}
} // namespace erlang_aot
