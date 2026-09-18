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

struct GuardConjunction {
    // Commas conjoin a nonempty ordered sequence; no guard legality is implied.
    std::vector<ExprId> tests;
    NodeSource source;
};

struct GuardSyntax {
    // Semicolons separate nonempty alternatives; absence of when uses optional instead.
    std::vector<GuardConjunction> alternatives;
    NodeSource source;
};

struct FunctionClause {
    // Arguments use restricted pattern syntax; guards and bodies retain expression syntax.
    std::vector<PatternSyntaxId> arguments;
    std::optional<GuardSyntax> guard;
    std::vector<ExprId> body;
    // Cover the complete named head through its final body expression.
    NodeSource source;
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
