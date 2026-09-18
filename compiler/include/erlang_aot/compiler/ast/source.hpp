#pragma once
#include <erlang_aot/compiler/ast/ids.hpp>
#include <erlang_aot/compiler/diagnostic.hpp>

namespace erlang_aot::ast {
struct TokenOrigin {
    // Preserve physical spelling independently of logical invocation coordinates.
    Span spelling;
    LogicalLocation location;
    // Keep the original ordered macro/include trace once per expanded token.
    std::vector<Span> related;
};

struct NodeSource {
    // Identify the owned per-form origin table, never a contiguous cross-file span.
    OriginId form;
    // Half-open expanded-token range; anchor may equal EOF for an empty construct.
    std::size_t begin;
    std::size_t end;
    std::size_t anchor;
};
} // namespace erlang_aot::ast
