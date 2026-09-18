#include "../encoding.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <iostream>

using namespace erlang_aot;
using test_records::hex;

// Exhaustive visitors expose only the Phase I projection shared with the OTP adapter.
struct ExpressionDump {
    void operator()(const ast::Atom &value) const { std::cout << "atom\t" << hex(utf8(value.name)) << '\n'; }

    void operator()(const ast::IntegerLiteral &value) const { std::cout << "integer\t" << value.value.decimal << '\n'; }

    void operator()(const ast::FloatLiteral &value) const {
        std::cout << "float\t" << test_records::float_bits(value.value) << '\n';
    }

    void operator()(const ast::CharacterLiteral &value) const {
        std::cout << "char\t" << static_cast<std::uint32_t>(value.value) << '\n';
    }

    void operator()(const ast::StringLiteral &value) const {
        std::cout << "string\t" << hex(utf8(value.value)) << '\n';
    }

    void operator()(const ast::Variable &) const { throw std::runtime_error("unmapped Phase I variable"); }
};

struct FormDump {
    // Borrow the immutable owner for safe traversal of body handles.
    const ast::Module &module;

    void operator()(const ast::ModuleAttribute &value) const {
        std::cout << "module\t" << hex(utf8(value.name.name)) << '\n';
    }

    void operator()(const ast::FileAttribute &value) const {
        const auto name = std::filesystem::path(utf8(value.name)).filename().string();
        std::cout << "file\t" << hex(name) << '\t' << value.line.decimal << '\n';
    }

    void operator()(const ast::ZeroArgumentFunction &value) const {
        if (value.body.size() != 1) {
            throw std::runtime_error("unmapped Phase I function body");
        }
        std::cout << "function\t" << hex(utf8(value.name.name)) << '\n';
        module.visit(value.body.front(), ExpressionDump{});
    }
};

// Dump native ASTs privately for tests; this is not a public intermediate representation.
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    SourceManager sources;
    PreprocessorSession preprocessor(sources.read(argv[1]));
    const auto result = parse_module(preprocessor);
    for (const auto &error : result.diagnostics) {
        std::cerr << render(error) << '\n';
    }
    if (result.failed) {
        return 1;
    }
    for (const auto &id : result.module.forms()) {
        result.module.visit(id, FormDump{result.module});
    }
}
