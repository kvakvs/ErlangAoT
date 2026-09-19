#pragma once
#include "ast/builder.hpp"
#include "parsing/operator_info.hpp"
#include "parsing/token_cursor.hpp"

namespace erlang_aot {
struct Value;

struct GrammarBudget {
    // Keep allocation and recursive grammar limits distinct at the call boundary.
    std::size_t nodes;
    std::size_t nesting;
    std::size_t &work;
};

struct GeneratorOperator {
    // Keep source kind and strictness independent of pattern legality.
    bool binary;
    bool strict;
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
    // Share work across successful and rejected forms; borrow tokens for diagnostic enrichment.
    std::size_t &work_;
    std::span<const Token> tokens_;
    // Charge grammar operations and attach actionable syntax context before rollback.
    void work(std::size_t amount = 1);
    void enrich(Diagnostic &diagnostic) const;
    [[noreturn]] void expected(std::string description) const;
    // Parse and terminate one form within the caller's AST transaction.
    ast::FormId complete_form();
    // Recognize module/file attributes and the currently supported function syntax.
    ast::FormValue attribute();
    // Specifications reuse function type products; constraints remain parser syntax.
    ast::Specification specification(bool callback);
    ast::SpecificationSignature signature();
    ast::TypeConstraint constraint();
    ast::TypeConstraint legacy_constraint(std::size_t begin);
    // Type grammar has separate precedence, payloads and category-safe child handles.
    ast::TypeId top_type();
    ast::TypeId type_expression(int minimum = 200);
    ast::TypeId type_prefix();
    ast::TypeValue type_primary();
    ast::TypeValue named_type();
    ast::TypeValue type_application(std::optional<ast::Atom> module, ast::Atom name,
                                    std::vector<ast::TypeId> arguments);
    ast::TypeValue hash_type();
    ast::ListType list_type();
    ast::BitstringType bitstring_type();
    std::pair<bool, ast::TypeId> binary_type_part();
    ast::FunType fun_type();
    ast::MapTypeField map_type_field();
    ast::RecordTypeField record_type_field();
    std::vector<ast::TypeId> type_elements(std::u32string_view close);
    ast::TypeId make_type(ast::TypeValue value, std::size_t begin, std::size_t anchor);
    ast::TypeDeclaration type_declaration(const ast::Atom &name, const ast::ExprId &head);
    ast::FormValue attribute_body(ast::Atom name, const std::vector<ast::ExprId> &arguments);
    ast::FormValue ordinary_attribute(ast::Atom name, const std::vector<ast::ExprId> &arguments);
    ast::FormValue checked_attribute(ast::Atom name, const std::vector<ast::ExprId> &arguments);
    ast::RecordDeclaration record_declaration();
    ast::RecordDeclarationField declaration_field();
    std::vector<ast::RecordDeclarationField> declaration_fields();
    ast::DocumentationAttribute documentation(bool module, const ast::ExprId &value);
    ast::TermId term(const ast::ExprId &expression, bool farity = true);
    ast::TermId term_value(const Value &value, const ast::NodeSource &source);
    ast::TermValue term_container(const Value &value, const ast::NodeSource &source);
    ast::TermValue term_sequence(const Value &value, const ast::NodeSource &source);
    std::vector<ast::DocumentationEntry> documentation_entries(const ast::MapExpression &value, bool module);
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
    ast::ExprValue hash(std::optional<ast::ExprId> base = {}, bool comprehension = true);
    ast::ExprValue map(std::optional<ast::ExprId> base, bool comprehension);
    ast::MapField map_field();
    ast::RecordIdentity record_identity();
    ast::ExprValue record(std::optional<ast::ExprId> base, ast::RecordIdentity identity);
    ast::ExprValue record_access(std::optional<ast::ExprId> base, ast::RecordIdentity identity);
    std::vector<ast::RecordField> record_fields();
    ast::RecordField record_field();
    ast::Atom record_name();
    void check_hash_base(const ast::ExprId &base, bool map, bool local) const;
    // Distinct bit_expr/bit_size_expr entries keep slash/colon outside general precedence.
    ast::ExprValue binary(bool comprehension = true);
    ast::BinarySegment binary_segment();
    ast::ExprId bit_value();
    ast::ExprId bit_primary();
    std::optional<std::vector<ast::BinaryModifier>> binary_modifiers();
    ast::BinaryModifier binary_modifier();
    ast::Bitstring binary_sigil(std::u32string content, std::size_t begin);
    ast::ExprValue literal();
    ast::ExprValue sigil();
    ast::Tuple tuple();
    ast::ExprValue list(bool comprehension);
    // Resolve aggregate/comprehension prefixes once, preserving strict and zipped qualifiers.
    void require_comprehension(bool allowed) const;
    ast::ExprValue maximum_hash();
    std::vector<ast::ComprehensionQualifier> qualifiers();
    ast::ComprehensionQualifier qualifier_group();
    ast::Qualifier qualifier();
    ast::Qualifier map_generator(ast::ExprId key, std::size_t begin);
    ast::Qualifier generator(ast::ExprId pattern, std::size_t begin);
    std::optional<GeneratorOperator> generator_operator() const;
    void require_binary_generator(const ast::ExprId &pattern, std::size_t begin);
    ast::ExprId binary_template(const std::vector<ast::BinarySegment> &segments) const;
    ast::PatternSyntaxId candidate(ast::ExprId expression);
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
