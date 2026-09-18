#pragma once
#include "ast/builder.hpp"
#include "parsing/operator_info.hpp"
#include "parsing/token_cursor.hpp"

namespace erlang_aot {
struct GrammarBudget {
    // Keep allocation and recursive grammar limits distinct at the call boundary.
    std::size_t nodes;
    std::size_t nesting;
};

// Construct only explicitly supported, complete syntax nodes.
class FormParser {
  public:
    // Borrow one expanded form and the current transaction's remaining node allowance.
    FormParser(std::span<const Token> tokens, const Token &end, ast::Builder &builder, GrammarBudget budget);
    ast::FormId parse();

  private:
    // Token positions index the active builder's owned origin table.
    TokenCursor cursor_;
    ast::Builder &builder_;
    std::size_t nodes_;
    std::size_t nesting_;
    std::size_t depth_ = 0;
    // Recognize module/file attributes and the currently supported function syntax.
    ast::FormValue attribute();
    ast::ModuleAttribute module_attribute();
    ast::FileAttribute file_attribute();
    ast::ZeroArgumentFunction function();
    // Parse bounded recursive values without rescanning expanded tokens.
    ast::ExprId expression(int minimum = 0);
    ast::ExprId prefix();
    ast::ExprId continuation(ast::ExprId left, const OperatorInfo &info);
    std::optional<OperatorInfo> next_operator() const;
    ast::ExprId make(ast::ExprValue value, std::size_t begin, std::size_t anchor);
    void nonassociative(const OperatorInfo &info) const;
    ast::ExprValue primary();
    ast::ExprValue literal();
    ast::ExprValue sigil();
    ast::Tuple tuple();
    ast::List list();
    std::vector<ast::ExprId> elements(std::u32string_view close);
    ast::ExprValue literal_value(const Token &token) const;
    // Match delimiters/category values without turning quoted atoms into syntax.
    void expect(std::u32string_view text);
    const Token &category(TokenKind kind, std::string_view description);
    void terminator();
    void node();
    [[noreturn]] void fail(DiagnosticCode code, std::string message) const;

    // Invalid decoded values represent a broken token contract, not Erlang syntax errors.
    template <typename Value> const Value &value(const Token &token) const {
        const auto *result = std::get_if<Value>(&token.value);
        if (!result) {
            throw token_diagnostic(DiagnosticCode::parser_contract, "inconsistent token kind/value", token);
        }
        return *result;
    }
};
} // namespace erlang_aot
