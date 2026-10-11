// Added for parse transforms: External Term Format and consult-text invariants against OTP-generated samples.
#include "transforms/etf.hpp"
#include "transforms/term_text.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace clause::transforms;

namespace {
// Read a whole fixture file as bytes.
std::string read_file(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    if (!file && !file.eof()) {
        throw std::runtime_error("cannot read " + path.string());
    }
    return text.str();
}

void require(const bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// The sample list round-trips through decode and encode byte for byte, as OTP wrote it.
TermId check_external(const std::string &bytes, Terms &terms) {
    const auto list = decode_external(bytes, terms);
    require(encode_external(terms, list) == bytes, "re-encoded samples differ from OTP's term_to_binary");
    return list;
}

// OTP's ~tp text of each sample reads back to the decoded sample, and Clause's own text reads back equal too.
void check_text(const std::string &text, Terms &terms, const TermId list) {
    clause::SourceManager sources;
    const auto read = read_text(sources.add("samples.txt", text), terms);
    const auto samples = terms.node(list).children_;
    require(read.size() == samples.size(), "sample count differs between text and binary");
    for (std::size_t index = 0; index < samples.size(); ++index) {
        require(equal(terms, read[index], terms, samples[index]), "OTP text of sample " + std::to_string(index));
        std::string own;
        write_text(own, terms, samples[index]);
        own += ".\n";
        const auto again = read_text(sources.add("own.txt", own), terms);
        require(again.size() == 1 && equal(terms, again[0], terms, samples[index]), "Clause text: " + own);
    }
}

// Every truncation and a corrupted tag fail with TermError, never a crash.
void check_malformed(const std::string &bytes) {
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        Terms terms;
        try {
            decode_external(bytes.substr(0, size), terms);
            throw std::runtime_error("truncated input of " + std::to_string(size) + " bytes decoded");
        } catch (const TermError &) {
        }
    }
    for (const std::string &bad : {std::string("\x83\x01", 2), std::string("\x83\x50\x00", 3), std::string("x")}) {
        Terms terms;
        try {
            decode_external(bad, terms);
            throw std::runtime_error("unsupported tag decoded");
        } catch (const TermError &) {
        }
    }
    clause::SourceManager sources;
    for (const auto *text : {"{a,b", "[1|2,3].", "#{a}.", "<<1:65>>.", "X.", "{a}"}) {
        Terms terms;
        try {
            read_text(sources.add("bad.txt", text), terms);
            throw std::runtime_error(std::string("malformed text read: ") + text);
        } catch (const TermError &) {
        }
    }
}

// Nesting far beyond any native stack encodes, decodes, prints, reads and compares without recursion.
void check_depth() {
    constexpr int DEPTH = 200000;
    Terms terms;
    auto inner = terms.nil();
    for (int level = 0; level < DEPTH; ++level) {
        inner = terms.tuple({terms.atom(U"cons"), inner});
    }
    const auto bytes = encode_external(terms, inner);
    Terms decoded;
    const auto copy = decode_external(bytes, decoded);
    require(equal(terms, inner, decoded, copy), "deep term changed through external format");
    std::string text;
    write_text(text, decoded, copy);
    text += ".\n";
    clause::SourceManager sources;
    const auto read = read_text(sources.add("deep.txt", text), decoded);
    require(read.size() == 1 && equal(decoded, read[0], terms, inner), "deep term changed through text");
}
} // namespace

int main(const int argc, char **argv) {
    try {
        require(argc == 2, "usage: transforms_terms <fixture directory>");
        const std::filesystem::path directory = argv[1];
        Terms terms;
        const auto bytes = read_file(directory / "samples.etf");
        const auto list = check_external(bytes, terms);
        check_text(read_file(directory / "samples.txt"), terms, list);
        check_malformed(bytes);
        check_depth();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "transforms_terms: ok\n";
    return 0;
}
