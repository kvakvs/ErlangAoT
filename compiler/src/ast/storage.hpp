#pragma once
#include "arena.hpp"
#include <erlang_aot/compiler/ast/module.hpp>

namespace erlang_aot::ast::detail {
struct OriginTable {
    // Token origins are owned once per form; empty ranges use the separate EOF origin.
    std::vector<TokenOrigin> tokens;
    TokenOrigin eof;
};

struct Storage {
    // Keep category arenas and committed source-order roots under one identity.
    std::shared_ptr<const Owner> owner = std::make_shared<const Owner>();
    Arena<Expression, ExprTag> expressions{owner};
    Arena<Form, FormTag> forms{owner};
    Arena<OriginTable, OriginTag> origins{owner};
    std::vector<FormId> roots;
};

// Validate a node's half-open range and anchor before any origin-table indexing.
const OriginTable &source_table(const Storage &storage, const NodeSource &source);
} // namespace erlang_aot::ast::detail
