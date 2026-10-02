#pragma once
#include "match_plan.hpp"

namespace erlang_aot::semantic {
struct PatternVisit {
    // Match a normalized source node against one candidate; literal prefixes defer their terminus to suffix.
    ast::ExprId id;
    std::size_t input;
    std::optional<ast::ExprId> suffix = {};
};

using MatchTask = std::variant<PatternVisit, MatchNode>;

struct MatchPlanner {
    // Index immutable normalization and clause-local binding events under one shared work ceiling.
    const Module &module;
    const Reporter &out;
    unsigned bits;
    std::map<const ast::Expression *, const NormalizedPattern *> patterns;
    std::map<const ast::Expression *, const Binding *> bindings;
    MatchPlan plan;
    std::size_t work = 0;
    std::size_t limit;

    // Charge indexing, scheduling and emission before publishing any partial plan.
    bool spend(const ast::ExprId &site);
    // First definitions publish tentative slots; repeated occurrences test the existing binding value.
    void variable(const NormalizedPattern &pattern, std::size_t input);
    // Emit scalar constraints; aliases and containers are scheduled by the iterative task walker.
    bool node(const NormalizedPattern &pattern, std::size_t input);
};

// Schedule each required map key independently, preserving duplicate value constraints.
bool expand_map(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                std::vector<MatchTask> &pending);

// Recognize only the container forms admitted by the shared checked runtime services.
bool container_pattern(const NormalizedPattern &pattern);
// Schedule shape/extraction and child constraints without recursive calls or unbounded pending work.
bool expand_container(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                      std::vector<MatchTask> &pending);
} // namespace erlang_aot::semantic
