#pragma once
#include "ast/builder.hpp"
#include "parsing/token_cursor.hpp"

namespace erlang_aot {
// Phase I form grammar constructs only explicitly supported, complete syntax nodes.
class FormParser {
  public:
    // Borrow one expanded form and the current transaction's remaining node allowance.
    FormParser(std::span<const Token> tokens, const Token &end, ast::Builder &builder, std::size_t nodes);
    ast::FormId parse();

  private:
    // Token positions index the active builder's owned origin table.
    TokenCursor cursor_;
    ast::Builder &builder_;
    std::size_t nodes_;
    // Recognize only Phase I module/file attributes and zero-argument scalar functions.
    ast::FormValue attribute();
    ast::ModuleAttribute module_attribute();
    ast::FileAttribute file_attribute();
    ast::ZeroArgumentFunction function();
    ast::ExprId literal();
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
