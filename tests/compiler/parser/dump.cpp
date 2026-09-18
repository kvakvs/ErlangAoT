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

    void identity(const ast::RecordIdentity &value) const {
        std::visit([&](const auto &name) { record_name(name); }, value.value);
    }

    void record_name(const ast::UnresolvedRecordName &value) const {
        std::cout << "record_local\t" << hex(utf8(value.name.name)) << '\n';
    }

    void record_name(const ast::QualifiedRecordName &value) const {
        std::cout << "record_qualified\t" << hex(utf8(value.module.name)) << '\t' << hex(utf8(value.name.name)) << '\n';
    }

    void record_name(const ast::InferredRecordName &) const { std::cout << "record_inferred\n"; }

    void operator()(const ast::MapExpression &value) const {
        std::cout << "map\t" << bool(value.base) << '\t' << value.fields.size() << '\n';
        if (value.base)
            child(*value.base);
        for (const auto &field : value.fields) {
            std::cout << "map_field\t" << (field.kind == ast::MapFieldKind::associate ? "=>" : ":=") << '\n';
            child(field.key);
            child(field.value);
        }
    }

    void operator()(const ast::RecordExpression &value) const {
        std::cout << "record\t" << bool(value.base) << '\t' << value.fields.size() << '\n';
        if (value.base)
            child(*value.base);
        identity(value.identity);
        for (const auto &field : value.fields) {
            std::cout << "record_field\n";
            std::visit(*this, field.name);
            child(field.value);
        }
    }

    void operator()(const ast::RecordAccess &value) const {
        std::cout << "record_access\n";
        child(value.base);
        identity(value.identity);
        (*this)(value.field);
    }

    void operator()(const ast::RecordIndex &value) const {
        std::cout << "record_index\n";
        (*this)(value.record);
        (*this)(value.field);
    }

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

    // Retain the earlier projection for the exact abstract shape used by binary sigils.
    const ast::StringLiteral *utf8_string(const ast::Bitstring &value) const {
        if (value.segments.size() != 1)
            return nullptr;
        const auto &segment = value.segments.front();
        if (segment.size || !segment.modifiers || segment.modifiers->size() != 1)
            return nullptr;
        const auto &modifier = segment.modifiers->front();
        if (modifier.name.name != U"utf8" || modifier.parameter)
            return nullptr;
        auto id = segment.value;
        while (const auto *group = std::get_if<ast::Group>(&module.expression(id).value))
            id = group->expression;
        return std::get_if<ast::StringLiteral>(&module.expression(id).value);
    }

    void segment(const ast::BinarySegment &value) const {
        std::cout << "segment\t" << bool(value.size) << '\t';
        if (value.modifiers)
            std::cout << value.modifiers->size();
        else
            std::cout << "default";
        std::cout << '\n';
        child(value.value);
        if (value.size)
            child(*value.size);
        if (value.modifiers) {
            for (const auto &modifier : *value.modifiers) {
                std::cout << "modifier\t" << hex(utf8(modifier.name.name)) << '\t';
                if (modifier.parameter)
                    std::cout << modifier.parameter->decimal;
                else
                    std::cout << "none";
                std::cout << '\n';
            }
        }
    }

    void operator()(const ast::Bitstring &value) const {
        if (const auto *string = utf8_string(value)) {
            std::cout << "binary_sigil\t" << hex(utf8(string->value)) << '\n';
            return;
        }
        std::cout << "bitstring\t" << value.segments.size() << '\n';
        for (const auto &item : value.segments)
            segment(item);
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

    void clause(const ast::FunctionClause &value) const {
        std::cout << "clause\t" << value.arguments.size() << '\t'
                  << (value.guard ? value.guard->alternatives.size() : 0) << '\t' << value.body.size() << '\n';
        for (const auto &id : value.arguments) {
            const auto &pattern = std::get<ast::RestrictedPattern>(module.pattern(id).value);
            module.visit(pattern.expression, ExpressionDump{module});
        }
        if (value.guard) {
            for (const auto &alternative : value.guard->alternatives) {
                std::cout << "guard\t" << alternative.tests.size() << '\n';
                for (const auto &id : alternative.tests)
                    module.visit(id, ExpressionDump{module});
            }
        }
        for (const auto &id : value.body)
            module.visit(id, ExpressionDump{module});
    }

    void operator()(const ast::Function &value) const {
        const auto &first = value.clauses.front();
        if (value.clauses.size() == 1 && first.arguments.empty() && !first.guard && first.body.size() == 1) {
            std::cout << "function\t" << hex(utf8(value.name.name)) << '\n';
            module.visit(first.body.front(), ExpressionDump{module});
            return;
        }
        std::cout << "function_full\t" << hex(utf8(value.name.name)) << '\t' << first.arguments.size() << '\t'
                  << value.clauses.size() << '\n';
        for (const auto &item : value.clauses)
            clause(item);
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
