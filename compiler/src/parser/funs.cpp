#include "forms.hpp"

namespace erlang_aot {
std::variant<ast::Atom, ast::Variable> FormParser::atom_or_variable() {
    if (cursor_.anchor().kind == TokenKind::variable)
        return ast::Variable{value<std::u32string>(*cursor_.consume())};
    return ast::Atom{value<std::u32string>(category(TokenKind::atom, "atom or variable"))};
}

std::variant<Integer, ast::Variable> FormParser::fun_arity() {
    if (cursor_.anchor().kind == TokenKind::variable)
        return ast::Variable{value<std::u32string>(*cursor_.consume())};
    return value<Integer>(category(TokenKind::integer, "fun reference arity"));
}

std::optional<ast::Variable> FormParser::fun_name() {
    if (cursor_.anchor().kind == TokenKind::variable)
        return ast::Variable{value<std::u32string>(*cursor_.consume())};
    return std::nullopt;
}

ast::ExprValue FormParser::fun_expression() {
    const auto *next = cursor_.peek(1);
    if (!next || (!syntax(*next, U"/") && !syntax(*next, U":")))
        return fun_clauses();
    auto first = atom_or_variable();
    if (cursor_.take_syntax(U":")) {
        auto name = atom_or_variable();
        expect(U"/");
        return ast::RemoteFunReference{std::move(first), std::move(name), fun_arity()};
    }
    const auto *name = std::get_if<ast::Atom>(&first);
    if (!name)
        fail(DiagnosticCode::parser_syntax, "local fun reference requires an atom name");
    expect(U"/");
    return ast::LocalFunReference{*name, value<Integer>(category(TokenKind::integer, "local fun arity"))};
}

ast::FunExpression FormParser::fun_clauses() {
    const auto begin = cursor_.offset();
    auto name = fun_name();
    std::vector<ast::FunctionClause> clauses;
    clauses.push_back(clause(begin));
    const auto arity = clauses.front().arguments.size();
    while (cursor_.take_syntax(U";")) {
        const auto start = cursor_.offset();
        const auto site = cursor_.anchor();
        auto next = fun_name();
        auto current = clause(start);
        check_clause(next ? next->name : U"", name ? name->name : U"", arity, current, site);
        clauses.push_back(std::move(current));
    }
    expect(U"end");
    return {std::move(name), std::move(clauses)};
}
} // namespace erlang_aot
