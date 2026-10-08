#pragma once
#include <clause/compiler/ast/expressions.hpp>

namespace clause::ast {
struct RestrictedPattern {
    // Parsed through pat_expr; nested containers still admit general expressions in OTP.
    ExprId expression;
};

struct PatternCandidate {
    // General expression syntax in permissive pattern positions requires later validation.
    ExprId expression;
};

using PatternValue = std::variant<RestrictedPattern, PatternCandidate>;

struct PatternSyntax {
    // Distinguish the grammar entry point without duplicating expression payload families.
    PatternValue value;
    NodeSource source;
};
} // namespace clause::ast
