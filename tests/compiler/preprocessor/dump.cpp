#include "../encoding.hpp"
#include <bit>
#include <clause/compiler/preprocessor.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

using test_records::hex;

// Normalize fixture paths while preserving exact literal values.
std::string value(const clause::Token &token, const std::string &directory) {
    if (const auto *n = std::get_if<clause::Integer>(&token.value)) {
        return hex(n->decimal);
    }
    if (const auto *n = std::get_if<double>(&token.value)) {
        return test_records::float_bits(*n);
    }
    std::string text = clause::utf8(token.text());
    if (const auto position = text.find(directory); position != std::string::npos) {
        // Fixture-path separators differ by host; literal values outside that prefix stay exact.
        text = std::filesystem::path(text).generic_string();
        text.replace(position, directory.size(), "<FIXTURES>");
    }
    return hex(text);
}

// Emit ordinary tokens while omitting the preprocessor's host-specific file attributes.
void print_form(const clause::OrdinaryForm &form, const std::string &directory) {
    if (form.tokens.size() > 1 && form.tokens[1].text() == U"file") {
        return;
    }
    constexpr const char *kinds[]{"atom",         "var",          "integer", "float",  "char", "string",
                                  "sigil_prefix", "sigil_suffix", "keyword", "symbol", "dot",  "comment"};
    for (const auto &token : form.tokens) {
        std::cout << kinds[static_cast<std::size_t>(token.kind)] << '\t' << value(token, directory) << '\n';
    }
}

// Project source preprocessing into stable records for golden and live OTP comparisons.
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    clause::SourceManager sources;
    clause::PreprocessorSession session(sources.read(argv[1]));
    const auto directory = std::filesystem::path(argv[1]).parent_path().string();
    while (const auto event = session.next()) {
        if (const auto *error = std::get_if<clause::Diagnostic>(&*event)) {
            std::cout << (error->severity == clause::Severity::warning ? "warning" : "error") << '\n';
            std::cerr << clause::render(*error) << '\n';
        }
        if (const auto *form = std::get_if<clause::OrdinaryForm>(&*event)) {
            print_form(*form, directory);
        }
    }
}
