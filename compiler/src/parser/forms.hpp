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
    ast::Function function();
    ast::FunctionClause clause(std::size_t begin);
    std::vector<ast::PatternSyntaxId> arguments();
    ast::PatternSyntaxId pattern(bool permissive = false);
    ast::GuardSyntax guard(std::size_t begin);
    std::optional<ast::GuardSyntax> optional_guard();
    std::vector<ast::ExprId> sequence();
    // Reuse sequence/guard parsing while each construct owns its closing delimiters.
    std::optional<ast::ExprValue> control(OperatorContext context);
    ast::BlockExpression block();
    ast::CaseExpression case_expression();
    ast::IfExpression if_expression();
    ast::ReceiveExpression receive_expression();
    std::vector<ast::BranchClause> branches();
    ast::BranchClause branch();
    // Fun heads reuse clauses; catches and maybe matches keep their grammar contexts.
    ast::ExprValue fun_expression();
    ast::FunExpression fun_clauses();
    std::optional<ast::Variable> fun_name();
    std::variant<ast::Atom, ast::Variable> atom_or_variable();
    std::variant<Integer, ast::Variable> fun_arity();
    void check_clause(std::u32string_view actual, std::u32string_view expected, std::size_t arity,
                      const ast::FunctionClause &clause, const Token &site) const;
    ast::TryExpression try_expression();
    ast::CatchClause catch_clause();
    ast::MaybeExpression maybe_expression();
    std::variant<ast::ExprId, ast::MaybeMatch> maybe_item();
    // Parse bounded recursive values without rescanning expanded tokens.
    ast::ExprId expression(int minimum = 0, OperatorContext context = OperatorContext::expression);
    ast::ExprId prefix(OperatorContext context);
    ast::ExprId continuation(ast::ExprId left, const OperatorInfo &info, OperatorContext context);
    std::optional<OperatorInfo> next_operator(OperatorContext context) const;
    ast::ExprId make(ast::ExprValue value, std::size_t begin, std::size_t anchor);
    void nonassociative(const OperatorInfo &info, OperatorContext context) const;
    ast::ExprValue primary(OperatorContext context);
    // Hash productions have restricted postfix bases independent of general calls/operators.
    ast::ExprId structural(OperatorContext context);
    ast::ExprId hash_suffix(ast::ExprId base);
    ast::ExprValue hash(std::optional<ast::ExprId> base = {});
    ast::MapExpression map(std::optional<ast::ExprId> base);
    ast::MapField map_field();
    ast::RecordIdentity record_identity();
    ast::ExprValue record(std::optional<ast::ExprId> base, ast::RecordIdentity identity);
    ast::ExprValue record_access(std::optional<ast::ExprId> base, ast::RecordIdentity identity);
    std::vector<ast::RecordField> record_fields();
    ast::RecordField record_field();
    ast::Atom record_name();
    void check_hash_base(const ast::ExprId &base, bool map, bool local) const;
    // Distinct bit_expr/bit_size_expr entries keep slash/colon outside general precedence.
    ast::Bitstring binary();
    ast::BinarySegment binary_segment();
    ast::ExprId bit_value();
    ast::ExprId bit_primary();
    std::optional<std::vector<ast::BinaryModifier>> binary_modifiers();
    ast::BinaryModifier binary_modifier();
    ast::Bitstring binary_sigil(std::u32string content, std::size_t begin);
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
    // Share the nesting bound across ordinary expressions and restricted binary primaries.
    void enter();
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
