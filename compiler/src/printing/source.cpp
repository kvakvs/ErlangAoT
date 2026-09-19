#include "token_text.hpp"
#include <erlang_aot/compiler/printing.hpp>
#include <ostream>
#include <stdexcept>

namespace erlang_aot {
namespace {
// Sigil bodies survive preprocessing unchanged; retain raw/triple delimiters and escapes.
void print_sigil(std::ostream &output, std::span<const Token> tokens) {
    if (tokens.size() < 3 || tokens[1].kind != TokenKind::string || tokens[2].kind != TokenKind::sigil_suffix) {
        throw std::invalid_argument("incomplete sigil in preprocessed form");
    }
    const auto &body = tokens[1].spelling;
    const auto spelling = std::u32string_view(body.source->text).substr(body.begin, body.end - body.begin);
    output << '~' << utf8(tokens[0].text()) << utf8(spelling) << utf8(tokens[2].text());
}

// Whitespace after a record-access dot would change it into a form terminator.
bool needs_space(const Token &previous) { return previous.kind != TokenKind::symbol || previous.text() != U"."; }
} // namespace

void print_preprocessed(std::ostream &output, const OrdinaryForm &form) {
    const std::span<const Token> tokens = form.tokens;
    std::size_t index = 0;
    while (index < tokens.size()) {
        if (index != 0 && needs_space(tokens[index - 1])) {
            output << ' ';
        }
        if (tokens[index].kind == TokenKind::sigil_prefix) {
            print_sigil(output, tokens.subspan(index));
            index += 3;
        } else {
            output << utf8(token_text(tokens[index]));
            ++index;
        }
    }
    output << '\n';
}
} // namespace erlang_aot
