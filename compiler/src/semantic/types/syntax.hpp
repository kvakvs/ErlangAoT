#pragma once
#include "domain.hpp"
#include <clause/compiler/ast/module.hpp>

namespace clause::semantic::types {
struct Description {
    // Preserve syntax children separately until they have semantic identities.
    Node node;
    std::vector<ast::TypeId> children = {};
};

// Exhaustive syntax description retains unresolved application and operator identities.
Description describe(const ast::TypeValue &value);
// Translate a bounded syntax graph iteratively; provenance stays on the borrowed AST.
Id translate(Graph &graph, const ast::Module &syntax, const ast::TypeId &root);
} // namespace clause::semantic::types
