#pragma once
// Added for parse transforms: shared state of the AST-to-abstract-format writer.
#include "export.hpp"
#include <clause/compiler/ast/module.hpp>

namespace clause::transforms {
// Writes AST nodes as abstract format terms; annotations follow erl_parse's choice of token per construct.
class AbstractWriter {
  public:
    AbstractWriter(const ast::Module &syntax, Terms &terms) : syntax_(syntax), terms_(terms) {}

    // Whole forms and their parts.
    TermId form(const ast::Form &form);
    TermId expression(const ast::ExprId &id);
    TermId pattern(const ast::PatternSyntaxId &id);
    TermId expressions(const std::vector<ast::ExprId> &items);
    // A guard as a list of alternatives, each a list of tests; [] when absent.
    TermId guard(const std::optional<ast::GuardSyntax> &guard);
    // {clause, Anno, Patterns, Guard, Body} with the given annotation and patterns.
    TermId clause(TermId anno, std::vector<TermId> patterns, const std::optional<ast::GuardSyntax> &guard,
                  const std::vector<ast::ExprId> &body);
    // Function and fun clauses: annotated at their first token (the name or the argument parenthesis).
    TermId function_clause(const ast::FunctionClause &clause);
    // Case, receive, try-of and maybe-else clauses: annotated at their pattern.
    TermId branch_clauses(const std::vector<ast::BranchClause> &clauses);
    TermId type(const ast::TypeId &id);
    TermId types(const std::vector<ast::TypeId> &items);
    // The plain term of a literal attribute value.
    TermId literal(const ast::TermId &id);

    // Annotations: {Line, Column} of a location, of a node's first token that is not an opening parenthesis (OTP's
    // first_anno), of the token at an absolute index of a node's form, or a node's parser anchor.
    TermId anno(const LogicalLocation &location);
    TermId first(const ast::NodeSource &source);
    TermId token(const ast::NodeSource &source, std::size_t index);
    TermId anchor(const ast::NodeSource &source);
    // The text of the token at an absolute index of a node's form.
    std::u32string_view token_text(const ast::NodeSource &source, std::size_t index) const;
    // The latest annotation inside an exported pattern (OTP's last_anno).
    TermId last_anno(TermId pattern);

    // Abstract nodes: {Tag, Anno, Rest...}, {atom, Anno, Name}, {var, Anno, Name}.
    TermId node(std::u32string_view tag, TermId anno, std::vector<TermId> rest);
    TermId atom_node(TermId anno, std::u32string_view name);
    TermId var_node(TermId anno, std::u32string_view name);

    const ast::Module &syntax() const { return syntax_; }

    Terms &terms() { return terms_; }

  private:
    // The syntax being exported and the arena receiving the forms.
    const ast::Module &syntax_;
    Terms &terms_;
};

// erl_parse's operator atoms (export_expressions.cpp).
std::u32string_view operator_name(ast::BinaryOperator operation);
std::u32string_view operator_name(ast::UnaryOperator operation);

// Category writers implemented beside their syntax (export_*.cpp).
TermId export_attribute(AbstractWriter &writer, const ast::Form &form);
TermId export_control(AbstractWriter &writer, const ast::Expression &expression);
TermId export_comprehension(AbstractWriter &writer, const ast::Expression &expression);
TermId export_specification(AbstractWriter &writer, const ast::Form &form, const ast::Specification &value);
TermId export_type_declaration(AbstractWriter &writer, const ast::Form &form, const ast::TypeDeclaration &value);
TermId export_record_fields(AbstractWriter &writer, const ast::RecordDeclaration &value);
// {type, A, fun, [{type, A, product, Args} | {type, A, any}, Result]} of a signature or fun type.
TermId export_fun_type(AbstractWriter &writer, TermId anno, const ast::FunType &value);
} // namespace clause::transforms
