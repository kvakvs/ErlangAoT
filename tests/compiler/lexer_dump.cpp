#include <erlang_aot/compiler/lexer.hpp>
#include <bit>
#include <iomanip>
#include <iostream>
#include <sstream>

// Encode arbitrary token text without ambiguity from tabs, quotes, or newlines.
std::string hex(std::string_view bytes)
{
    if (bytes.empty()) { return "-"; }
    std::ostringstream output;
    for (const unsigned char byte : bytes) { output << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte); }
    return output.str();
}
// Preserve floating-point bits and arbitrary-precision integer digits.
std::string value(const erlang_aot::Token& token)
{
    if (const auto* integer = std::get_if<erlang_aot::Integer>(&token.value)) { return hex(integer->decimal); }
    if (const auto* floating = std::get_if<double>(&token.value)) {
        std::ostringstream output;
        output << std::hex << std::setw(16) << std::setfill('0') << std::bit_cast<std::uint64_t>(*floating);
        return output.str();
    }
    return hex(erlang_aot::utf8(token.text()));
}
// Produce private scanner-oracle records, not public intermediate-stage output.
int main(int argc, char* argv[])
{
    if (argc != 2) { return 2; }
    erlang_aot::SourceManager sources;
    erlang_aot::Lexer lexer(sources.read(argv[1]), true);
    constexpr std::string_view kinds[]{"atom", "var", "integer", "float", "char", "string",
        "sigil_prefix", "sigil_suffix", "keyword", "symbol", "dot", "comment"};
    try {
        while (const auto token = lexer.next()) {
            std::cout << kinds[static_cast<std::size_t>(token->kind)] << '\t' << token->location.line
                << '\t' << token->location.column << '\t' << value(*token) << '\n';
        }
    } catch (const erlang_aot::LexicalError& error) {
        std::cerr << erlang_aot::render(error.diagnostic) << '\n'; return 1;
    }
}
