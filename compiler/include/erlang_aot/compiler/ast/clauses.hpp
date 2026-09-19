#pragma once
#include <erlang_aot/compiler/ast/source.hpp>
#include <optional>
#include <vector>

namespace erlang_aot::ast {
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

} // namespace erlang_aot::ast
