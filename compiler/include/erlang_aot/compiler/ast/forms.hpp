#pragma once
#include <erlang_aot/compiler/ast/expressions.hpp>

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

struct ZeroArgumentFunction {
    // Phase I's explicit subset; full argument/guard/clause syntax is added in step 7.
    Atom name;
    // A checked nonempty body holds expression handles in source order.
    std::vector<ExprId> body;
};

using FormValue = std::variant<ModuleAttribute, FileAttribute, ZeroArgumentFunction>;

struct Form {
    // Keep form identity, ordered syntax, and provenance together.
    FormValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
