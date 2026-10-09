#include "source_printer.hpp"
#include "token_text.hpp"
#include <array>
#include <ostream>
#include <utility>

namespace clause::printing {
namespace {
// Whether a form is a function.
bool function_form(const ast::Form &form) { return std::holds_alternative<ast::Function>(form.value); }

// Whether a form is a -spec, which stays with the function after it.
bool specification_form(const ast::Form &form) {
    const auto *specification = std::get_if<ast::Specification>(&form.value);
    return specification && !specification->callback;
}

// A blank line sets each function, with the -spec right before it, apart from other forms.
bool blank_between(const ast::Form &previous, const ast::Form &form) {
    if (function_form(previous)) {
        return true;
    }
    return (function_form(form) || specification_form(form)) && !specification_form(previous);
}

// The code point of the valid UTF-8 sequence at `at`, advancing `at` past it.
char32_t next_code(std::string_view text, std::size_t &at) {
    const auto lead = static_cast<unsigned char>(text[at++]);
    if (lead < 0x80) {
        return lead;
    }
    const std::size_t continuation = lead < 0xE0 ? 1 : lead < 0xF0 ? 2 : 3;
    char32_t code = lead & (0x3FU >> continuation);
    for (std::size_t i = 0; i < continuation && at < text.size(); ++i) {
        code = (code << 6U) | (static_cast<unsigned char>(text[at++]) & 0x3FU);
    }
    return code;
}

// The -file(Source, 1) form the preprocessor puts before a module's first form; the source has none.
bool preprocessor_start(const ast::Form &form) {
    const auto *file = std::get_if<ast::FileAttribute>(&form.value);
    return file && file->line.decimal == "1";
}

// Comment lines above a form, one per note line.
std::string comments(const SourceNotes &notes, const ast::Form &form) {
    std::string result;
    if (notes.form) {
        for (const auto &line : notes.form(form)) {
            result += "%% " + line + '\n';
        }
    }
    return result;
}
} // namespace

std::string atom_text(const ast::Atom &atom) { return utf8(token_text(TokenKind::atom, atom.name)); }

std::string literal_text(TokenKind kind, const TokenValue &value) { return utf8(token_text(kind, value)); }

std::string operator_text(ast::BinaryOperator operation) {
    static constexpr std::array<std::string_view, 27> spellings{
        "!", "orelse", "andalso", "==",  "/=",  "=<", "<",   ">=", ">", "=:=", "=/=", "++",   "--", "+",
        "-", "bor",    "bxor",    "bsl", "bsr", "or", "xor", "*",  "/", "div", "rem", "band", "and"};
    return std::string(spellings.at(static_cast<std::size_t>(operation)));
}

std::string operator_text(ast::UnaryOperator operation) {
    static constexpr std::array<std::string_view, 4> spellings{"+", "-", "bnot", "not"};
    return std::string(spellings.at(static_cast<std::size_t>(operation)));
}

std::string trailing_notes(std::string_view text) {
    std::string result;
    std::string note;
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] == NOTE_START) {
            const auto end = text.find(NOTE_END, at);
            note = std::string(text.substr(at + 1, end - at - 1));
            at = end;
        } else {
            if (text[at] == '\n' && !note.empty()) {
                result += " % " + std::exchange(note, {});
            }
            result += text[at];
        }
    }
    return note.empty() ? result : result + " % " + note;
}

std::string unary_text(ast::UnaryOperator operation, const std::string &operand) {
    const auto spelling = operator_text(operation);
    // Word operators need a space; a sign before another sign would lex as ++ or --.
    const bool word = spelling.size() > 1;
    const bool sign = !operand.empty() && (operand.front() == '+' || operand.front() == '-');
    return spelling + (word || sign ? " " : "") + operand;
}
} // namespace clause::printing

namespace clause {
void print_source(std::ostream &output, const ast::Module &module, const SourceNotes &notes) {
    const printing::SourcePrinter printer(module, notes);
    const ast::Form *previous = nullptr;
    for (const auto &id : module.forms()) {
        const auto &form = module.form(id);
        if (!previous && printing::preprocessor_start(form)) {
            continue;
        }
        if (previous && printing::blank_between(*previous, form)) {
            output << '\n';
        }
        output << printing::comments(notes, form) << printing::trailing_notes(printer.form(form)) << '\n';
        previous = &form;
    }
}

std::string expression_source(const ast::Module &module, const ast::ExprId &id) {
    const SourceNotes notes;
    return printing::SourcePrinter(module, notes).expression(id, {.annotated = false});
}

std::string atom_source(std::string_view name) {
    // Names come from decoded atoms, so they are valid UTF-8.
    std::u32string decoded;
    for (std::size_t at = 0; at < name.size();) {
        decoded += printing::next_code(name, at);
    }
    return printing::atom_text({decoded});
}

std::string operator_source(ast::BinaryOperator operation) { return printing::operator_text(operation); }

std::string operator_source(ast::UnaryOperator operation) { return printing::operator_text(operation); }

std::string type_source(const ast::Module &module, const ast::TypeId &id) {
    const SourceNotes notes;
    return printing::SourcePrinter(module, notes).type(id);
}
} // namespace clause
