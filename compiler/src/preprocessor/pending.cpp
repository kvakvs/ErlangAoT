#include "engine.hpp"

namespace erlang_aot {
// Feature-sensitive lexing is connected in step 11.
void PreprocessorSession::State::keywords() {}

// Context builtins and function recognition are connected in step 9.
std::vector<Token> PreprocessorSession::State::expand(std::span<const Token> tokens, bool) {
    return MacroExpander(macros, options.limits, [](const Token &) -> std::optional<std::vector<Token>> { return {}; })
        .expand(tokens);
}

void PreprocessorSession::State::ordinary(std::vector<Token> tokens) {
    pending.emplace_back(OrdinaryForm{std::move(tokens)});
}

void PreprocessorSession::State::emit_file(const Token &site, const std::string &name, std::size_t line) {
    auto tokens = fragment("-file(\"\",1).");
    Source converted(0, "<filename>", name);
    tokens[3] = generated(site, TokenKind::string, converted.text);
    tokens[5] = generated(site, TokenKind::integer, Integer{std::to_string(line)});
    for (auto &token : tokens) {
        token.location = site.location;
        token.origins.push_back(site.spelling);
    }
    pending.emplace_back(OrdinaryForm{std::move(tokens)});
}
} // namespace erlang_aot
