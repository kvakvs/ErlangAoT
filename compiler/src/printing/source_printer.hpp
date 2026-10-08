#pragma once
#include <clause/compiler/printing.hpp>
#include <cstddef>
#include <string>
#include <vector>

// Erlang source text of parsed syntax (docs/compile.md#source-printing): forms one after another, clauses and
// block expressions on indented lines, everything else on one line. Parentheses come only from the syntax's own
// groups, so the printed text parses back to the same tree.
namespace clause::printing {
// Where an expression is printed: its line indentation, and whether it is a whole body expression or may carry an
// annotation at all (patterns and guards do not).
struct Place {
    std::size_t indent = 0;
    bool statement = false;
    bool annotated = true;
};

class SourcePrinter final {
  public:
    // Borrow the syntax and the optional notes for one module.
    SourcePrinter(const ast::Module &syntax, const SourceNotes &notes) : syntax_(syntax), notes_(notes) {}

    // The text of one form, ending in its full stop.
    std::string form(const ast::Form &form) const;
    // The text of an expression at `place`, with its annotation when the notes give one.
    std::string expression(const ast::ExprId &id, Place place) const;
    // A pattern: an expression without annotations.
    std::string pattern(const ast::PatternSyntaxId &id, std::size_t indent) const;
    // A guard: alternatives separated by `;`, tests by `,`.
    std::string guard(const ast::GuardSyntax &guard, std::size_t indent) const;
    // Body expressions, one per line at `indent`, separated by commas.
    std::string body(const std::vector<ast::ExprId> &body, std::size_t indent) const;
    // A type, a literal term.
    std::string type(const ast::TypeId &id) const;
    std::string term(const ast::TermId &id) const;
    // Expressions separated by ", " on one line, as operands of an expression at `place`.
    std::string list(const std::vector<ast::ExprId> &items, Place place) const;
    // Types separated by ", ".
    std::string types(const std::vector<ast::TypeId> &items) const;

    const ast::Module &syntax() const { return syntax_; }

  private:
    // The syntax being printed and the caller's notes.
    const ast::Module &syntax_;
    const SourceNotes &notes_;
};

// Spaces for an indentation level.
inline std::string spaces(std::size_t indent) { return std::string(indent, ' '); }

// An atom as source text, quoted when it needs to be.
std::string atom_text(const ast::Atom &atom);
// An integer, float, character or string literal as source text.
std::string literal_text(TokenKind kind, const TokenValue &value);
// The spelling of an operator.
std::string operator_text(ast::BinaryOperator operation);
std::string operator_text(ast::UnaryOperator operation);
// A unary operator applied to an operand's text, spaced so the result lexes back the same.
std::string unary_text(ast::UnaryOperator operation, const std::string &operand);

// Clauses of case, receive, try's of and maybe's else parts, one per line group, separated by `;`.
std::string branch_clauses(const SourcePrinter &printer, const std::vector<ast::BranchClause> &clauses,
                           std::size_t indent);
// The text of a block expression family (case, if, receive, try, maybe, begin, fun): several lines.
std::string control_text(const SourcePrinter &printer, const ast::ExprValue &value, std::size_t indent);
// Whether an expression is a block printed on several lines by control_text.
bool control(const ast::ExprValue &value);
// The text of a comprehension.
std::string comprehension_text(const SourcePrinter &printer, const ast::ExprValue &value, std::size_t indent);
} // namespace clause::printing
