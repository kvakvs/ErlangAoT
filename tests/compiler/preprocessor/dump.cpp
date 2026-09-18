#include "../encoding.hpp"
#include <bit>
#include <erlang_aot/compiler/preprocessor.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

using test_records::hex;

// Normalize fixture paths while preserving exact literal values.
std::string value(const erlang_aot::Token &token, const std::string &directory) {
    if (const auto *n = std::get_if<erlang_aot::Integer>(&token.value)) {
        return hex(n->decimal);
    }
    if (const auto *n = std::get_if<double>(&token.value)) {
        return test_records::float_bits(*n);
    }
    std::string text = erlang_aot::utf8(token.text());
    if (const auto position = text.find(directory); position != std::string::npos) {
        text.replace(position, directory.size(), "<FIXTURES>");
    }
    return hex(text);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession session(sources.read(argv[1]));
    const auto directory = std::filesystem::path(argv[1]).parent_path().string();
    constexpr const char *kinds[]{"atom",         "var",          "integer", "float",  "char", "string",
                                  "sigil_prefix", "sigil_suffix", "keyword", "symbol", "dot",  "comment"};
    while (const auto event = session.next()) {
        if (const auto *error = std::get_if<erlang_aot::Diagnostic>(&*event)) {
            std::cout << (error->severity == erlang_aot::Severity::warning ? "warning" : "error") << '\n';
            std::cerr << erlang_aot::render(*error) << '\n';
        }
        if (const auto *form = std::get_if<erlang_aot::OrdinaryForm>(&*event)) {
            if (form->tokens.size() > 1 && form->tokens[1].text() == U"file") {
                continue;
            }
            for (const auto &token : form->tokens) {
                std::cout << kinds[static_cast<std::size_t>(token.kind)] << '\t' << value(token, directory) << '\n';
            }
        }
    }
}
