#pragma once
#include <erlang_aot/compiler/ast/patterns.hpp>
#include <erlang_aot/compiler/ast/terms.hpp>

namespace erlang_aot::ast {
struct ModuleAttribute {
    // Preserve the declared name without enforcing module-level semantic rules.
    Atom name;
    // Legacy parameterized modules remain distinguishable from an ordinary declaration.
    std::optional<std::vector<Variable>> parameters = {};
};

struct FileAttribute {
    // Retain explicit and preprocessor-generated file mappings without applying them again.
    std::u32string name;
    Integer line;
};

struct Function {
    // All clauses share the name and first clause's arity; the builder checks this invariant.
    Atom name;
    std::vector<FunctionClause> clauses;
};

struct NameArity {
    // Parser syntax accepts arbitrary integer arities; semantic bounds are deferred.
    Atom name;
    Integer arity;
};

struct ExportAttribute {
    // Preserve export order and duplicates for later validation.
    std::vector<NameArity> functions;
};

struct ImportAttribute {
    // Module qualification applies to the complete ordered import list.
    Atom module;
    std::vector<NameArity> functions;
};

struct ImportRecordAttribute {
    // Imported native record names remain unresolved.
    Atom module;
    std::vector<Atom> names;
};

struct GenericAttribute {
    // Includes compile, behaviour, export_type and other attributes that OTP treats as literal data.
    Atom name;
    TermId value;
};

struct RecordDeclarationField {
    // Keep default expressions unevaluated and preserve declaration order.
    Atom name;
    std::optional<ExprId> default_value;
    NodeSource source;
};

struct RecordDeclaration {
    // Native and tuple records share fields but retain their distinct declaration category.
    Atom name;
    bool native;
    std::vector<RecordDeclarationField> fields;
};

struct DocumentationEntry {
    // Only doc metadata's equiv call may contain executable syntax.
    TermId key;
    std::variant<TermId, ExprId> value;
};

struct DocumentationAttribute {
    // File references remain literal tuples; no documentation files are opened here.
    bool module;
    std::variant<TermId, std::vector<DocumentationEntry>> value;
};

using FormValue = std::variant<ModuleAttribute, FileAttribute, Function, ExportAttribute, ImportAttribute,
                               ImportRecordAttribute, GenericAttribute, RecordDeclaration, DocumentationAttribute>;

struct Form {
    // Keep form identity, ordered syntax, and provenance together.
    FormValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
