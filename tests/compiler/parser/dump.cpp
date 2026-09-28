#include "../encoding.hpp"
#include "operators.hpp"
#include "terms_dump.hpp"
#include "types_dump.hpp"
#include <cstdio>
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
        test_records::records.node("zip", (value.qualifiers.size()), " qualifiers=", value.qualifiers.size());
        for (const auto &item : value.qualifiers) {
            qualifier(item);
        }
    }

    void qualifiers(const std::vector<ast::ComprehensionQualifier> &values) const {
        for (const auto &item : values) {
            std::visit([&](const auto &value) { qualifier(value); }, item);
        }
    }

    void operator()(const ast::FilterQualifier &value) const {
        test_records::records.node("filter", 1);
        child(value.expression);
    }

    void operator()(const ast::ListGenerator &value) const {
        test_records::records.node("generate", 2, " operator=", (value.strict ? "<:-" : "<-"));
        pattern(value.pattern);
        child(value.input);
    }

    void operator()(const ast::BinaryGenerator &value) const {
        test_records::records.node("b_generate", 2, " operator=", (value.strict ? "<:=" : "<="));
        pattern(value.pattern);
        child(value.input);
    }

    void operator()(const ast::MapGenerator &value) const {
        test_records::records.node("m_generate", 3, " operator=", (value.strict ? "<:-" : "<-"));
        pattern(value.key);
        pattern(value.value);
        child(value.input);
    }

    void operator()(const ast::ListComprehension &value) const {
        test_records::records.node("lc", (value.templates.size()) + (value.qualifiers.size()),
                                   " templates=", value.templates.size(), " qualifiers=", value.qualifiers.size());
        expressions(value.templates);
        qualifiers(value.qualifiers);
    }

    void operator()(const ast::MapComprehension &value) const {
        test_records::records.node("mc", (value.templates.size()) + (value.qualifiers.size()),
                                   " templates=", value.templates.size(), " qualifiers=", value.qualifiers.size());
        for (const auto &field : value.templates) {
            test_records::records.node("map_field", 2,
                                       " operator=", (field.kind == ast::MapFieldKind::associate ? "=>" : ":="));
            child(field.key);
            child(field.value);
        }
        qualifiers(value.qualifiers);
    }

    void operator()(const ast::BinaryComprehension &value) const {
        test_records::records.node("bc", 1 + (value.qualifiers.size()), " qualifiers=", value.qualifiers.size());
        child(value.expression);
        qualifiers(value.qualifiers);
    }

    // Reference arity integers reuse the same literal projection as expression integers.
    void operator()(const Integer &value) const { (*this)(ast::IntegerLiteral{value}); }

    void operator()(const ast::LocalFunReference &value) const {
        test_records::records.node("local_fun", 2);
        (*this)(value.name);
        (*this)(value.arity);
    }

    void operator()(const ast::RemoteFunReference &value) const {
        test_records::records.node("remote_fun", 3);
        std::visit(*this, value.module);
        std::visit(*this, value.name);
        std::visit(*this, value.arity);
    }

    void function_clause(const ast::FunctionClause &value) const {
        test_records::records.node(
            "clause",
            (value.arguments.size()) + ((value.guard ? value.guard->alternatives.size() : 0)) + (value.body.size()),
            " arguments=", value.arguments.size(), " guards=", (value.guard ? value.guard->alternatives.size() : 0),
            " body=", value.body.size());
        for (const auto &id : value.arguments) {
            pattern(id);
        }
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::FunExpression &value) const {
        test_records::records.node("fun", (value.clauses.size()),
                                   " name_hex=", (value.name ? hex(utf8(value.name->name)) : "-"),
                                   " clauses=", value.clauses.size());
        for (const auto &clause : value.clauses) {
            function_clause(clause);
        }
    }

    void handler(const ast::CatchClause &value) const {
        test_records::records.node("clause",
                                   (1) + ((value.guard ? value.guard->alternatives.size() : 0)) + (value.body.size()),
                                   " arguments=", 1, " guards=", (value.guard ? value.guard->alternatives.size() : 0),
                                   " body=", value.body.size());
        test_records::records.node("tuple", (3), " elements=", 3);
        if (value.exception_class) {
            std::visit(*this, *value.exception_class);
        } else {
            (*this)(ast::Atom{U"throw"});
        }
        pattern(value.reason);
        (*this)(value.stacktrace.value_or(ast::Variable{U"_"}));
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::TryExpression &value) const {
        const auto clauses = value.of ? value.of->size() : 0;
        const auto handlers = value.handlers ? value.handlers->size() : 0;
        const auto after = value.after ? value.after->size() : 0;
        test_records::records.node("try", value.body.size() + clauses + handlers + after, " body=", value.body.size(),
                                   " clauses=", clauses, " handlers=", handlers, " after=", after);
        expressions(value.body);
        if (value.of) {
            for (const auto &item : *value.of) {
                branch(item);
            }
        }
        if (value.handlers) {
            for (const auto &item : *value.handlers) {
                handler(item);
            }
        }
        if (value.after) {
            expressions(*value.after);
        }
    }

    void maybe_item(const ast::ExprId &value) const { child(value); }

    void maybe_item(const ast::MaybeMatch &value) const {
        test_records::records.node("maybe_match", 2);
        pattern(value.pattern);
        child(value.value);
    }

    void operator()(const ast::MaybeExpression &value) const {
        test_records::records.node("maybe", (value.body.size()) + ((value.otherwise ? value.otherwise->size() : 0)),
                                   " body=", value.body.size(),
                                   " clauses=", (value.otherwise ? value.otherwise->size() : 0));
        for (const auto &item : value.body) {
            std::visit([&](const auto &part) { maybe_item(part); }, item);
        }
        if (value.otherwise) {
            for (const auto &item : *value.otherwise) {
                branch(item);
            }
        }
    }

    // Share sequence and guard projection across control and function-like clauses.
    void expressions(const std::vector<ast::ExprId> &ids) const {
        for (const auto &id : ids) {
            child(id);
        }
    }

    void pattern(const ast::PatternSyntaxId &id) const {
        std::visit([&](const auto &value) { child(value.expression); }, module.pattern(id).value);
    }

    void guards(const std::optional<ast::GuardSyntax> &value) const {
        if (!value) {
            return;
        }
        for (const auto &alternative : value->alternatives) {
            test_records::records.node("guard", (alternative.tests.size()), " tests=", alternative.tests.size());
            expressions(alternative.tests);
        }
    }

    void branch(const ast::BranchClause &value) const {
        test_records::records.node("clause",
                                   (1) + ((value.guard ? value.guard->alternatives.size() : 0)) + (value.body.size()),
                                   " arguments=", 1, " guards=", (value.guard ? value.guard->alternatives.size() : 0),
                                   " body=", value.body.size());
        pattern(value.pattern);
        guards(value.guard);
        expressions(value.body);
    }

    void operator()(const ast::BlockExpression &value) const {
        test_records::records.node("block", (value.body.size()), " body=", value.body.size());
        expressions(value.body);
    }

    void operator()(const ast::CaseExpression &value) const {
        test_records::records.node("case", 1 + (value.clauses.size()), " clauses=", value.clauses.size());
        child(value.value);
        for (const auto &clause : value.clauses) {
            branch(clause);
        }
    }

    void operator()(const ast::IfExpression &value) const {
        test_records::records.node("if", (value.clauses.size()), " clauses=", value.clauses.size());
        for (const auto &clause : value.clauses) {
            test_records::records.node("clause", (0) + (clause.guard.alternatives.size()) + (clause.body.size()),
                                       " arguments=", 0, " guards=", clause.guard.alternatives.size(),
                                       " body=", clause.body.size());
            guards(clause.guard);
            expressions(clause.body);
        }
    }

    void operator()(const ast::ReceiveExpression &value) const {
        test_records::records.node("receive", value.clauses.size() + std::size_t{2} * bool(value.after),
                                   " clauses=", value.clauses.size(), " timeout=", bool(value.after));
        for (const auto &clause : value.clauses) {
            branch(clause);
        }
        if (value.after) {
            child(value.after->timeout);
            test_records::records.node("after", (value.after->body.size()), " body=", value.after->body.size());
            expressions(value.after->body);
        }
    }

    void identity(const ast::RecordIdentity &value) const {
        std::visit([&](const auto &name) { record_name(name); }, value.value);
    }

    void record_name(const ast::UnresolvedRecordName &value) const {
        test_records::records.node("record_local", 0, " name_hex=", hex(utf8(value.name.name)));
    }

    void record_name(const ast::QualifiedRecordName &value) const {
        test_records::records.node("record_qualified", 0, " module_hex=", hex(utf8(value.module.name)),
                                   " name_hex=", hex(utf8(value.name.name)));
    }

    void record_name(const ast::InferredRecordName &) const { test_records::records.node("record_inferred", 0); }

    void operator()(const ast::MapExpression &value) const {
        test_records::records.node("map", (bool(value.base)) + (value.fields.size()), " base=", bool(value.base),
                                   " fields=", value.fields.size());
        if (value.base) {
            child(*value.base);
        }
        for (const auto &field : value.fields) {
            test_records::records.node("map_field", 2,
                                       " operator=", (field.kind == ast::MapFieldKind::associate ? "=>" : ":="));
            child(field.key);
            child(field.value);
        }
    }

    void operator()(const ast::RecordExpression &value) const {
        test_records::records.node("record", 1 + (bool(value.base)) + (value.fields.size()), " base=", bool(value.base),
                                   " fields=", value.fields.size());
        if (value.base) {
            child(*value.base);
        }
        identity(value.identity);
        for (const auto &field : value.fields) {
            test_records::records.node("record_field", 2);
            std::visit(*this, field.name);
            child(field.value);
        }
    }

    void operator()(const ast::RecordAccess &value) const {
        test_records::records.node("record_access", 3);
        child(value.base);
        identity(value.identity);
        (*this)(value.field);
    }

    void operator()(const ast::RecordIndex &value) const {
        test_records::records.node("record_index", 2);
        (*this)(value.record);
        (*this)(value.field);
    }

    void operator()(const ast::UnaryExpression &value) const {
        test_records::records.node("unary", 1, " operator=", spelling(value.operation));
        child(value.operand);
    }

    void operator()(const ast::BinaryExpression &value) const {
        test_records::records.node("binary", 2, " operator=", spelling(value.operation));
        child(value.left);
        child(value.right);
    }

    void operator()(const ast::MatchExpression &value) const {
        test_records::records.node("match", 2);
        child(value.left);
        child(value.right);
    }

    void operator()(const ast::CatchExpression &value) const {
        test_records::records.node("catch", 1);
        child(value.expression);
    }

    void operator()(const ast::RemoteExpression &value) const {
        test_records::records.node("remote", 2);
        child(value.module);
        child(value.function);
    }

    void operator()(const ast::CallExpression &value) const {
        test_records::records.node("call", 1 + (value.arguments.size()), " arguments=", value.arguments.size());
        child(value.target);
        for (const auto &id : value.arguments) {
            child(id);
        }
    }

    void operator()(const ast::Tuple &value) const {
        test_records::records.node("tuple", (value.elements.size()), " elements=", value.elements.size());
        for (const auto &id : value.elements) {
            child(id);
        }
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
            if (!rest) {
                break;
            }
            elements.insert(elements.end(), rest->elements.begin(), rest->elements.end());
            tail = rest->tail;
        }
        test_records::records.node("list", (elements.size()) + (bool(tail)), " elements=", elements.size(),
                                   " tail=", bool(tail));
        for (const auto &id : elements) {
            child(id);
        }
        if (tail) {
            child(*tail);
        }
    }

    void operator()(const ast::Group &value) const { child(value.expression); }

    // Retain the earlier projection for the exact abstract shape used by binary sigils.
    const ast::StringLiteral *utf8_string(const ast::Bitstring &value) const {
        if (value.segments.size() != 1) {
            return nullptr;
        }
        const auto &segment = value.segments.front();
        if (segment.size || !segment.modifiers || segment.modifiers->size() != 1) {
            return nullptr;
        }
        const auto &modifier = segment.modifiers->front();
        if (modifier.name.name != U"utf8" || modifier.parameter) {
            return nullptr;
        }
        auto id = segment.value;
        while (const auto *group = std::get_if<ast::Group>(&module.expression(id).value)) {
            id = group->expression;
        }
        return std::get_if<ast::StringLiteral>(&module.expression(id).value);
    }

    void segment(const ast::BinarySegment &value) const {
        test_records::records.node("segment", 1 + bool(value.size) + (value.modifiers ? value.modifiers->size() : 0),
                                   " size=", bool(value.size), " modifiers=",
                                   (value.modifiers ? std::to_string(value.modifiers->size()) : "default"));
        child(value.value);
        if (value.size) {
            child(*value.size);
        }
        if (value.modifiers) {
            for (const auto &modifier : *value.modifiers) {
                test_records::records.node("modifier", 0, " name_hex=", hex(utf8(modifier.name.name)),
                                           " parameter=", (modifier.parameter ? modifier.parameter->decimal : "none"));
            }
        }
    }

    void operator()(const ast::Bitstring &value) const {
        if (const auto *string = utf8_string(value)) {
            test_records::records.node("binary_sigil", 0, " value_hex=", hex(utf8(string->value)));
            return;
        }
        test_records::records.node("bitstring", (value.segments.size()), " segments=", value.segments.size());
        for (const auto &item : value.segments) {
            segment(item);
        }
    }

    void operator()(const ast::Atom &value) const {
        test_records::records.node("atom", 0, " name_hex=", hex(utf8(value.name)));
    }

    void operator()(const ast::IntegerLiteral &value) const {
        test_records::records.node("integer", 0, " value=", value.value.decimal);
    }

    void operator()(const ast::FloatLiteral &value) const {
        test_records::records.node("float", 0, " bits=", test_records::float_bits(value.value));
    }

    void operator()(const ast::CharacterLiteral &value) const {
        test_records::records.node("char", 0, " value=", static_cast<std::uint32_t>(value.value));
    }

    void operator()(const ast::StringLiteral &value) const {
        test_records::records.node("string", 0, " value_hex=", hex(utf8(value.value)));
    }

    void operator()(const ast::Variable &value) const {
        test_records::records.node("var", 0, " name_hex=", hex(utf8(value.name)));
    }
};

struct FormDump {
    // Borrow the immutable owner for safe traversal of body handles.
    const ast::Module &module;

    void operator()(const ast::Specification &value) const {
        test_records::records.node("spec", (value.signatures.size()), " callback=", value.callback,
                                   " module_hex=", (value.module ? hex(utf8(value.module->name)) : "-"),
                                   " name_hex=", hex(utf8(value.name.name)), " arity=", value.arity,
                                   " signatures=", value.signatures.size());
        for (const auto &signature : value.signatures) {
            test_records::records.node("signature", 1 + (signature.constraints.size()),
                                       " constraints=", signature.constraints.size());
            TypeDump{module}(signature.function);
            for (const auto &constraint : signature.constraints) {
                test_records::records.node("constraint", 2);
                TypeDump{module}(constraint.variable);
                module.visit(constraint.bound, TypeDump{module});
            }
        }
    }

    void operator()(const ast::TypeDeclaration &value) const {
        test_records::records.node("type_decl", 1 + (value.parameters.size()),
                                   " kind=", static_cast<unsigned>(value.kind),
                                   " name_hex=", hex(utf8(value.name.name)), " parameters=", value.parameters.size());
        for (const auto &parameter : value.parameters) {
            TypeDump{module}(parameter);
        }
        module.visit(value.type, TypeDump{module});
    }

    void operator()(const ast::ModuleAttribute &value) const {
        if (value.parameters) {
            test_records::records.node("legacy_module", (value.parameters->size()),
                                       " name_hex=", hex(utf8(value.name.name)),
                                       " parameters=", value.parameters->size());
            for (const auto &parameter : *value.parameters) {
                test_records::records.node("var", 0, " name_hex=", hex(utf8(parameter.name)));
            }
            return;
        }
        test_records::records.node("module", 0, " name_hex=", hex(utf8(value.name.name)));
    }

    void operator()(const ast::FileAttribute &value) const {
        const auto name = std::filesystem::path(utf8(value.name)).filename().string();
        test_records::records.node("file", 0, " name_hex=", hex(name), " line=", value.line.decimal);
    }

    void arities(const std::vector<ast::NameArity> &values) const {
        for (const auto &value : values) {
            test_records::records.node("arity", 0, " name_hex=", hex(utf8(value.name.name)),
                                       " value=", value.arity.decimal);
        }
    }

    void operator()(const ast::ExportAttribute &value) const {
        test_records::records.node("export", (value.functions.size()), " functions=", value.functions.size());
        arities(value.functions);
    }

    void operator()(const ast::ImportAttribute &value) const {
        test_records::records.node("import", (value.functions.size()), " module_hex=", hex(utf8(value.module.name)),
                                   " functions=", value.functions.size());
        arities(value.functions);
    }

    void operator()(const ast::ImportRecordAttribute &value) const {
        test_records::records.node("import_record", (value.names.size()), " module_hex=", hex(utf8(value.module.name)),
                                   " names=", value.names.size());
        for (const auto &name : value.names) {
            ExpressionDump{module}(name);
        }
    }

    void operator()(const ast::GenericAttribute &value) const {
        test_records::records.node("attribute", 1, " name_hex=", hex(utf8(value.name.name)));
        module.visit(value.value, TermDump{module});
    }

    void operator()(const ast::RecordDeclaration &value) const {
        test_records::records.node("record_decl", (value.fields.size()), " name_hex=", hex(utf8(value.name.name)),
                                   " native=", value.native, " fields=", value.fields.size());
        for (const auto &field : value.fields) {
            if (field.type) {
                test_records::records.node("typed_field", 2);
                module.visit(*field.type, TypeDump{module});
            }
            test_records::records.node("record_decl_field", (field.default_value.has_value()),
                                       " name_hex=", hex(utf8(field.name.name)),
                                       " default=", field.default_value.has_value());
            if (field.default_value) {
                module.visit(*field.default_value, ExpressionDump{module});
            }
        }
    }

    void operator()(const ast::DocumentationAttribute &value) const {
        test_records::records.node("doc", 1, " module=", value.module);
        if (const auto *literal = std::get_if<ast::TermId>(&value.value)) {
            module.visit(*literal, TermDump{module});
            return;
        }
        const auto &entries = std::get<std::vector<ast::DocumentationEntry>>(value.value);
        test_records::records.node("metadata", 2 * (entries.size()), " entries=", entries.size());
        for (const auto &entry : entries) {
            module.visit(entry.key, TermDump{module});
            if (const auto *literal = std::get_if<ast::TermId>(&entry.value)) {
                module.visit(*literal, TermDump{module});
            } else {
                test_records::records.node("equiv", 1);
                module.visit(std::get<ast::ExprId>(entry.value), ExpressionDump{module});
            }
        }
    }

    void clause(const ast::FunctionClause &value) const {
        test_records::records.node(
            "clause",
            (value.arguments.size()) + ((value.guard ? value.guard->alternatives.size() : 0)) + (value.body.size()),
            " arguments=", value.arguments.size(), " guards=", (value.guard ? value.guard->alternatives.size() : 0),
            " body=", value.body.size());
        for (const auto &id : value.arguments) {
            const auto &pattern = std::get<ast::RestrictedPattern>(module.pattern(id).value);
            module.visit(pattern.expression, ExpressionDump{module});
        }
        if (value.guard) {
            for (const auto &alternative : value.guard->alternatives) {
                test_records::records.node("guard", (alternative.tests.size()), " tests=", alternative.tests.size());
                for (const auto &id : alternative.tests) {
                    module.visit(id, ExpressionDump{module});
                }
            }
        }
        for (const auto &id : value.body) {
            module.visit(id, ExpressionDump{module});
        }
    }

    void operator()(const ast::Function &value) const {
        const auto &first = value.clauses.front();
        if (value.clauses.size() == 1 && first.arguments.empty() && !first.guard && first.body.size() == 1) {
            test_records::records.node("function", 1, " name_hex=", hex(utf8(value.name.name)));
            module.visit(first.body.front(), ExpressionDump{module});
            return;
        }
        test_records::records.node("function_full", (value.clauses.size()), " name_hex=", hex(utf8(value.name.name)),
                                   " arity=", first.arguments.size(), " clauses=", value.clauses.size());
        for (const auto &item : value.clauses) {
            clause(item);
        }
    }
};

// Dump native ASTs privately for tests; this is not a public intermediate representation.
int main(int argc, char **argv) try {
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
    test_records::records.finish();
} catch (const std::exception &error) {
    std::fprintf(stderr, "AST dump failed: %s\n", error.what());
    return 2;
} catch (...) {
    std::fputs("AST dump failed: unknown exception\n", stderr);
    return 2;
}
