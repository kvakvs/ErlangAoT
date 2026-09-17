#include <erlang_aot/compiler/lexer.hpp>
#include <stdexcept>

// Keep test assertions active regardless of optimization mode.
void require(bool value) { if (!value) { throw std::runtime_error("lexer assertion failed"); } }

// Verify owned token lifetime, physical byte offsets, and logical remapping.
void source_locations()
{
    erlang_aot::SourceManager manager;
    auto source = manager.add("unicode.erl", "% λ\r\natom.");
    require(source->position(6).line == 2 && source->position(6).byte == 7);
    erlang_aot::Lexer lexer(source);
    lexer.set_location("logical.erl", 40);
    const auto token = lexer.next();
    require(token->location.file == "logical.erl" && token->location.line == 41);
    require(token->spelling.source->spelling(token->spelling.begin, token->spelling.end) == "atom");
    const auto latin = manager.add("latin.erl", "% coding: Latin-1\n'\xe9'.");
    require(latin->encoding == erlang_aot::Encoding::latin1);
    require(erlang_aot::Lexer(latin).next()->text() == U"é");
    try { manager.add("invalid.erl", "\xc0\x80"); require(false); }
    catch (const erlang_aot::EncodingError& error) { require(error.byte == 0); }
}

// Ensure forms leave later feature-sensitive text unscanned.
void incremental_forms()
{
    erlang_aot::SourceManager manager;
    erlang_aot::Lexer lexer(manager.add("forms.erl", "first(). maybe()."));
    require(lexer.form().back().kind == erlang_aot::TokenKind::dot);
    lexer.set_keywords({});
    require(lexer.form().front().kind == erlang_aot::TokenKind::atom);
    require(lexer.form().empty());
}

// Check lexical errors with stable categories, valid spans, and finite progress.
void invalid_literals()
{
    using enum erlang_aot::DiagnosticCode;
    const std::pair<std::string, erlang_aot::DiagnosticCode> cases[]{
        {"\"unterminated", unterminated_literal}, {"$", unterminated_literal},
        {"$\\x{}", invalid_escape}, {"$\\x{110000}", invalid_escape},
        {"1#0", invalid_number}, {"16#", invalid_number}, {"1.0e+", invalid_number},
        {"1_", invalid_number}, {"1a", invalid_number}, {"1.0foo", invalid_number},
        {"16#1.x", invalid_number}, {"16#1_.0", invalid_number},
        {"\"a\"\"b\"", adjacent_strings}, {"λ", invalid_character}};
    for (const auto& [input, code] : cases) {
        erlang_aot::SourceManager manager;
        erlang_aot::Lexer lexer(manager.add("bad.erl", input));
        try { while (lexer.next()) {} require(false); }
        catch (const erlang_aot::LexicalError& error) { require(error.diagnostic.code == code); }
    }
}
int main() { source_locations(); incremental_forms(); invalid_literals(); }
