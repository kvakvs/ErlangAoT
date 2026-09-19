#pragma once
#include <erlang_aot/compiler/ast/patterns.hpp>

namespace erlang_aot::ast {
struct ModuleAttribute {
    // Preserve the declared name without enforcing module-level semantic rules.
    Atom name;
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

using FormValue = std::variant<ModuleAttribute, FileAttribute, Function>;

struct Form {
    // Keep form identity, ordered syntax, and provenance together.
    FormValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
