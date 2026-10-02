#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
enum class MatchOperation : std::uint8_t { bind, exact_binding, exact_literal, extract, success, mismatch };
enum class EmptyValue : std::uint8_t { tuple, list };
using MatchLiteral = std::variant<std::int64_t, ast::Atom, EmptyValue>;

struct MatchNode {
    // Retain source and candidate input slot, with identities independent of spelling.
    ast::ExprId source;
    MatchOperation operation;
    std::size_t input;
    std::optional<BindingId> binding = {};
    std::optional<MatchLiteral> literal = {};
    // Each test has explicit success/mismatch continuations; terminals do not consume these edges.
    std::size_t success = 0;
    std::size_t mismatch = 0;
};

struct MatchPlan {
    // Candidate inputs are original arguments; bindings are tentative until the caller's success edge.
    std::size_t inputs;
    std::vector<BindingId> outputs;
    // Flat nodes reserve extraction for later checked container services, without admitting them now.
    std::vector<MatchNode> nodes;
};

struct MatchOptions {
    // Select one source clause and target word layout within a shared construction ceiling.
    std::size_t clause = 0;
    unsigned word_bits = 64;
    std::size_t work_limit = 100'000;
};

// Build a one-input body pattern with existing bindings preserved as exact constraints.
std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const ast::ExprId &pattern,
                                         const Reporter &out, MatchOptions options = {});
// Consume normalized semantics and binding events within explicit node/work ceilings.
std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const Reporter &out,
                                         MatchOptions options = {});
} // namespace erlang_aot::semantic
