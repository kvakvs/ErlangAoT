#include "../encoding.hpp"
#include "operators.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <iostream>

using namespace erlang_aot;
using test_records::hex;

// Exhaustive visitors expose the implemented syntax projection shared with the OTP adapter.
struct ExpressionDump {
    const ast::Module &module;

    void child(const ast::ExprId &id) const { module.visit(id, *this); }

    void operator()(const ast::UnaryExpression &value) const {
        std::cout << "unary\t" << spelling(value.operation) << '\n';
        child(value.operand);
    }

    void operator()(const ast::BinaryExpression &value) const {
        std::cout << "binary\t" << spelling(value.operation) << '\n';
        child(value.left);
        child(value.right);
    }

    void operator()(const ast::MatchExpression &value) const {
        std::cout << "match\n";
        child(value.left);
        child(value.right);
    }

    void operator()(const ast::CatchExpression &value) const {
        std::cout << "catch\n";
        child(value.expression);
    }

    void operator()(const ast::RemoteExpression &value) const {
        std::cout << "remote\n";
        child(value.module);
        child(value.function);
    }

    void operator()(const ast::CallExpression &value) const {
        std::cout << "call\t" << value.arguments.size() << '\n';
        child(value.target);
        for (const auto &id : value.arguments)
            child(id);
    }

    void operator()(const ast::Tuple &value) const {
        std::cout << "tuple\t" << value.elements.size() << '\n';
        for (const auto &id : value.elements)
            child(id);
    }

    void operator()(const ast::List &value) const {
        auto elements = value.elements;
        auto tail = value.tail;
        while (tail) {
            const auto &payload = module.expression(*tail).value;
            if (const auto *group = std::get_if<ast::Group>(&payload)) {
                tail = group->expression;
                continue;
            }
            const auto *rest = std::get_if<ast::List>(&payload);
            if (!rest)
                break;
            elements.insert(elements.end(), rest->elements.begin(), rest->elements.end());
            tail = rest->tail;
        }
        std::cout << "list\t" << elements.size() << '\t' << bool(tail) << '\n';
        for (const auto &id : elements)
            child(id);
        if (tail)
            child(*tail);
    }

    void operator()(const ast::Group &value) const { child(value.expression); }

    void operator()(const ast::BinarySigilLiteral &value) const {
        std::cout << "binary_sigil\t" << hex(utf8(value.value)) << '\n';
    }

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

    void operator()(const ast::Variable &value) const { std::cout << "var\t" << hex(utf8(value.name)) << '\n'; }
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
        module.visit(value.body.front(), ExpressionDump{module});
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
