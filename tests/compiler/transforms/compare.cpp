// Added for parse transforms: compare two consult files of abstract forms as terms, not as text layout.
#include "transforms/term_text.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace clause::transforms;

namespace {
std::string read_file(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot read " + path.string());
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// The canonical text of one term, for mismatch reports.
std::string text(const Terms &terms, const TermId id) {
    std::string out;
    write_text(out, terms, id);
    return out;
}
} // namespace

// transforms_compare EXPECTED ACTUAL: exit 0 when both hold equal terms in the same order.
int main(const int argc, char **argv) {
    try {
        if (argc != 3) {
            throw std::runtime_error("usage: transforms_compare <expected> <actual>");
        }
        clause::SourceManager sources;
        Terms terms;
        const auto expected = read_text(sources.add(argv[1], read_file(argv[1])), terms);
        const auto actual = read_text(sources.add(argv[2], read_file(argv[2])), terms);
        for (std::size_t index = 0; index < std::max(expected.size(), actual.size()); ++index) {
            if (index >= expected.size() || index >= actual.size() ||
                !equal(terms, expected[index], terms, actual[index])) {
                std::cerr << "form " << index + 1 << " differs\n  expected: "
                          << (index < expected.size() ? text(terms, expected[index]) : "<none>")
                          << "\n  actual:   " << (index < actual.size() ? text(terms, actual[index]) : "<none>")
                          << '\n';
                return 1;
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "transforms_compare: ok\n";
    return 0;
}
