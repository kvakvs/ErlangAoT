#pragma once
#include <clause/compiler/ast/module.hpp>
#include <clause/compiler/preprocessor.hpp>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clause {
// Write one expanded form as UTF-8 Erlang source, followed by a newline.
// Accepts semantic PreprocessorSession forms; retains sigils' original literal bodies.
void print_preprocessed(std::ostream &output, const OrdinaryForm &form);
// Write parenthesized objects with two-space indentation and named scalar/child fields.
// Deep trees use explicit depth labels after 64 levels to bound indentation cost.
// A scheduled-object budget also bounds repeated visits to shared syntax; exhaustion throws length_error.
void print_ast(std::ostream &output, const ast::Module &module, std::size_t visits = 4000000);

// Optional text added while printing source: an expression's note, printed as a trailing `% Text` comment on the
// line of a whole-line expression (a body expression or a case's scrutinee; the outermost one per line), and comment
// lines (without the leading %) written above a form.
struct SourceNotes {
    std::function<std::optional<std::string>(const ast::Expression &)> expression_ = {};
    std::function<std::vector<std::string>(const ast::Form &)> form_ = {};
    // The number of trailing forms left out, such as the compiler's generated module_info/0,1.
    std::size_t omitted_ = 0;
};

// Write every form of `module` as UTF-8 Erlang source, in form order. The text is the parsed syntax: macros are
// expanded and the original comments and layout are gone.
void print_source(std::ostream &output, const ast::Module &module, const SourceNotes &notes = {});
// The source text of one expression or type of `module`, on one line where it allows.
std::string expression_source(const ast::Module &module, const ast::ExprId &id);
std::string type_source(const ast::Module &module, const ast::TypeId &id);
// The source text of one literal term of `module`, such as an attribute value.
std::string term_source(const ast::Module &module, const ast::TermId &id);
// An atom name (UTF-8) as source text, quoted when it needs to be.
std::string atom_source(std::string_view name);
// Valid UTF-8 text as an Erlang string literal.
std::string string_source(std::string_view text);
// The spelling of an operator.
std::string operator_source(ast::BinaryOperator operation);
std::string operator_source(ast::UnaryOperator operation);
} // namespace clause
