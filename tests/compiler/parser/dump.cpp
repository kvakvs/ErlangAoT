#include "../encoding.hpp"
#include "operators.hpp"
#include "terms_dump.hpp"
#include "types_dump.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <iostream>

using namespace erlang_aot;
using test_records::hex;

// Exhaustive visitors expose the implemented syntax projection shared with the OTP adapter.
struct ExpressionDump {
    const ast::Module &module;

    void child(const ast::ExprId &id) const { module.visit(id, *this); }

    // Comprehension records keep template count and zipped group boundaries explicit.
    void qualifier(const ast::Qualifier &value) const { std::visit(*this, value.value); }

    void qualifier(const ast::ZippedQualifier &value) const {
        std::cout << "zip\t" << value.qualifiers.size() << '\n';
        for (const auto &item : value.qualifiers)
            qualifier(item);
    }

    void qualifiers(const std::vector<ast::ComprehensionQualifier> &values) const {
        for (const auto &item : values)
            std::visit([&](const auto &value) { qualifier(value); }, item);
    }

    void operator()(const ast::FilterQualifier &value) const {
        std::cout << "filter\n";
        child(value.expression);
    }

    void operator()(const ast::ListGenerator &value) const {
        std::cout << "generate\t" << (value.strict ? "<:-" : "<-") << '\n';
        pattern(value.pattern);
        child(value.input);
    }

    void operator()(const ast::BinaryGenerator &value) const {
        std::cout << "b_generate\t" << (value.strict ? "<:=" : "<=") << '\n';
        pattern(value.pattern);
        child(value.input);
    }

    void operator()(const ast::MapGenerator &value) const {
        std::cout << "m_generate\t" << (value.strict ? "<:-" : "<-") << '\n';
        pattern(value.key);
        pattern(value.value);
        child(value.input);
    }

    void operator()(const ast::ListComprehension &value) const {
        std::cout << "lc\t" << value.templates.size() << '\t' << value.qualifiers.size() << '\n';
        expressions(value.templates);
        qualifiers(value.qualifiers);
    }

    void operator()(const ast::MapComprehension &value) const {
        std::cout << "mc\t" << value.templates.size() << '\t' << value.qualifiers.size() << '\n';
        for (const auto &field : value.templates) {
            std::cout << "map_field\t" << (field.kind == ast::MapFieldKind::associate ? "=>" : ":=") << '\n';
            child(field.key);
            child(field.value);
        }
        qualifiers(value.qualifiers);
    }

    void operator()(const ast::BinaryComprehension &value) const {
        std::cout << "bc\t" << value.qualifiers.size() << '\n';
        child(value.expression);
        qualifiers(value.qualifiers);
    }

    // Reference arity integers reuse the same literal projection as expression integers.
    void operator()(const Integer &value) const { (*this)(ast::IntegerLiteral{value}); }

    void operator()(const ast::LocalFunReference &value) const {
        std::cout << "local_fun\n";
        (*this)(value.name);
        (*this)(value.arity);
    }

    void operator()(const ast::RemoteFunReference &value) const {
        std::cout << "remote_fun\n";
        std::visit(*this, value.module);
        std::visit(*this, value.name);
        std::visit(*this, value.arity);
    }

    void function_clause(const ast::FunctionClause &value) const {
        std::cout << "clause\t" << value.arguments.size() << '\t'
                  << (value.guard ? value.guard->alternatives.size() : 0) << '\t' << value.body.size() << '\n';
        for (const auto &id : value.arguments)
            pattern(id);
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::FunExpression &value) const {
        std::cout << "fun\t" << (value.name ? hex(utf8(value.name->name)) : "-") << '\t' << value.clauses.size()
                  << '\n';
        for (const auto &clause : value.clauses)
            function_clause(clause);
    }

    void handler(const ast::CatchClause &value) const {
        std::cout << "clause\t1\t" << (value.guard ? value.guard->alternatives.size() : 0) << '\t' << value.body.size()
                  << '\n';
        std::cout << "tuple\t3\n";
        if (value.exception_class)
            std::visit(*this, *value.exception_class);
        else
            (*this)(ast::Atom{U"throw"});
        pattern(value.reason);
        (*this)(value.stacktrace.value_or(ast::Variable{U"_"}));
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::TryExpression &value) const {
        std::cout << "try\t" << value.body.size() << '\t' << (value.of ? value.of->size() : 0) << '\t'
                  << (value.handlers ? value.handlers->size() : 0) << '\t' << (value.after ? value.after->size() : 0)
                  << '\n';
        expressions(value.body);
        if (value.of)
            for (const auto &item : *value.of)
                branch(item);
        if (value.handlers)
            for (const auto &item : *value.handlers)
                handler(item);
        if (value.after)
            expressions(*value.after);
    }

    void maybe_item(const ast::ExprId &value) const { child(value); }

    void maybe_item(const ast::MaybeMatch &value) const {
        std::cout << "maybe_match\n";
        pattern(value.pattern);
        child(value.value);
    }

    void operator()(const ast::MaybeExpression &value) const {
        std::cout << "maybe\t" << value.body.size() << '\t' << (value.otherwise ? value.otherwise->size() : 0) << '\n';
        for (const auto &item : value.body)
            std::visit([&](const auto &part) { maybe_item(part); }, item);
        if (value.otherwise)
            for (const auto &item : *value.otherwise)
                branch(item);
    }

    // Share sequence and guard projection across control and function-like clauses.
    void expressions(const std::vector<ast::ExprId> &ids) const {
        for (const auto &id : ids)
            child(id);
    }

    void pattern(const ast::PatternSyntaxId &id) const {
        std::visit([&](const auto &value) { child(value.expression); }, module.pattern(id).value);
    }

    void guards(const std::optional<ast::GuardSyntax> &value) const {
        if (!value)
            return;
        for (const auto &alternative : value->alternatives) {
            std::cout << "guard\t" << alternative.tests.size() << '\n';
            expressions(alternative.tests);
        }
    }

    void branch(const ast::BranchClause &value) const {
        std::cout << "clause\t1\t" << (value.guard ? value.guard->alternatives.size() : 0) << '\t' << value.body.size()
                  << '\n';
        pattern(value.pattern);
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::BlockExpression &value) const {
        std::cout << "block\t" << value.body.size() << '\n';
        expressions(value.body);
    }

    void operator()(const ast::CaseExpression &value) const {
        std::cout << "case\t" << value.clauses.size() << '\n';
        child(value.value);
        for (const auto &clause : value.clauses)
            branch(clause);
    }

    void operator()(const ast::IfExpression &value) const {
        std::cout << "if\t" << value.clauses.size() << '\n';
        for (const auto &clause : value.clauses) {
            std::cout << "clause\t0\t" << clause.guard.alternatives.size() << '\t' << clause.body.size() << '\n';
            guards(clause.guard);
            expressions(clause.body);
        }
    }

    void operator()(const ast::ReceiveExpression &value) const {
        std::cout << "receive\t" << value.clauses.size() << '\t' << bool(value.after) << '\n';
        for (const auto &clause : value.clauses)
            branch(clause);
        if (value.after) {
            child(value.after->timeout);
            std::cout << "after\t" << value.after->body.size() << '\n';
            expressions(value.after->body);
        }
    }

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

    void operator()(const ast::Specification &value) const {
        std::cout << "spec\t" << value.callback << '\t' << (value.module ? hex(utf8(value.module->name)) : "-") << '\t'
                  << hex(utf8(value.name.name)) << '\t' << value.arity << '\t' << value.signatures.size() << '\n';
        for (const auto &signature : value.signatures) {
            std::cout << "signature\t" << signature.constraints.size() << '\n';
            TypeDump{module}(signature.function);
            for (const auto &constraint : signature.constraints) {
                std::cout << "constraint\n";
                TypeDump{module}(constraint.variable);
                module.visit(constraint.bound, TypeDump{module});
            }
        }
    }

    void operator()(const ast::TypeDeclaration &value) const {
        std::cout << "type_decl\t" << static_cast<unsigned>(value.kind) << '\t' << hex(utf8(value.name.name)) << '\t'
                  << value.parameters.size() << '\n';
        for (const auto &parameter : value.parameters)
            TypeDump{module}(parameter);
        module.visit(value.type, TypeDump{module});
    }

    void operator()(const ast::ModuleAttribute &value) const {
        if (value.parameters) {
            std::cout << "legacy_module\t" << hex(utf8(value.name.name)) << '\t' << value.parameters->size() << '\n';
            for (const auto &parameter : *value.parameters)
                std::cout << "var\t" << hex(utf8(parameter.name)) << '\n';
            return;
        }
        std::cout << "module\t" << hex(utf8(value.name.name)) << '\n';
    }

    void operator()(const ast::FileAttribute &value) const {
        const auto name = std::filesystem::path(utf8(value.name)).filename().string();
        std::cout << "file\t" << hex(name) << '\t' << value.line.decimal << '\n';
    }

    void arities(const std::vector<ast::NameArity> &values) const {
        std::cout << values.size() << '\n';
        for (const auto &value : values)
            std::cout << hex(utf8(value.name.name)) << '\t' << value.arity.decimal << '\n';
    }

    void operator()(const ast::ExportAttribute &value) const {
        std::cout << "export\t";
        arities(value.functions);
    }

    void operator()(const ast::ImportAttribute &value) const {
        std::cout << "import\t" << hex(utf8(value.module.name)) << '\t';
        arities(value.functions);
    }

    void operator()(const ast::ImportRecordAttribute &value) const {
        std::cout << "import_record\t" << hex(utf8(value.module.name)) << '\t' << value.names.size() << '\n';
        for (const auto &name : value.names)
            ExpressionDump{module}(name);
    }

    void operator()(const ast::GenericAttribute &value) const {
        std::cout << "attribute\t" << hex(utf8(value.name.name)) << '\n';
        module.visit(value.value, TermDump{module});
    }

    void operator()(const ast::RecordDeclaration &value) const {
        std::cout << "record_decl\t" << hex(utf8(value.name.name)) << '\t' << value.native << '\t'
                  << value.fields.size() << '\n';
        for (const auto &field : value.fields) {
            if (field.type) {
                std::cout << "typed_field\n";
                module.visit(*field.type, TypeDump{module});
            }
            std::cout << "record_decl_field\t" << hex(utf8(field.name.name)) << '\t' << field.default_value.has_value()
                      << '\n';
            if (field.default_value)
                module.visit(*field.default_value, ExpressionDump{module});
        }
    }

    void operator()(const ast::DocumentationAttribute &value) const {
        std::cout << "doc\t" << value.module << '\n';
        if (const auto *literal = std::get_if<ast::TermId>(&value.value)) {
            module.visit(*literal, TermDump{module});
            return;
        }
        const auto &entries = std::get<std::vector<ast::DocumentationEntry>>(value.value);
        std::cout << "metadata\t" << entries.size() << '\n';
        for (const auto &entry : entries) {
            module.visit(entry.key, TermDump{module});
            if (const auto *literal = std::get_if<ast::TermId>(&entry.value))
                module.visit(*literal, TermDump{module});
            else {
                std::cout << "equiv\n";
                module.visit(std::get<ast::ExprId>(entry.value), ExpressionDump{module});
            }
        }
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
