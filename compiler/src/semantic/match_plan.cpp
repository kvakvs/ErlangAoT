#include "capabilities.hpp"
#include "features.hpp"
#include "match_plan_internal.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <erlang_aot/abi/term.hpp>
#include <span>

namespace erlang_aot::semantic {
namespace {
// Decode owned folded decimal text without arbitrary-size narrowing or host-width assumptions.
std::optional<MatchLiteral> integer(const ast::IntegerLiteral &integer, const unsigned bits) {
    std::int64_t number = 0;
    const auto &text = integer.value.decimal;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return integer;
    }
    const bool fits = bits == 32 ? abi::v1::IntegerEncoding<32>::encode(number).has_value()
                                 : bits == 64 && abi::v1::IntegerEncoding<64>::encode(number).has_value();
    return fits ? MatchLiteral{number} : MatchLiteral{integer};
}

// Canonical empty literals share immediate encodings with the checked container services.
std::optional<MatchLiteral> empty(const NormalizedPattern &pattern) {
    if (pattern.kind == PatternKind::tuple && pattern.children.empty()) {
        return EmptyValue::tuple;
    }
    if (pattern.kind == PatternKind::list && pattern.children.empty()) {
        return EmptyValue::list;
    }
    if (pattern.literal && std::holds_alternative<ast::StringLiteral>(*pattern.literal) &&
        std::get<ast::StringLiteral>(*pattern.literal).value.empty()) {
        return EmptyValue::list;
    }
    return {};
}

// Folded integers use immediates where possible and preserve decimal text for rooted bignum construction.
std::optional<MatchLiteral> literal(const NormalizedPattern &pattern, const unsigned bits) {
    if (const auto value = empty(pattern)) {
        return value;
    }
    if (!pattern.literal) {
        return {};
    }
    if (const auto *atom = std::get_if<ast::Atom>(&*pattern.literal)) {
        return *atom;
    }
    if (const auto *real = std::get_if<ast::FloatLiteral>(&*pattern.literal)) {
        return *real;
    }
    const auto *integer = std::get_if<ast::IntegerLiteral>(&*pattern.literal);
    return integer ? semantic::integer(*integer, bits) : std::nullopt;
}

} // namespace

bool MatchPlanner::spend(const ast::ExprId &site) {
    if (work++ >= limit) {
        report(module, &module.syntax->expression(site).source, "match plan work limit exceeded", out);
        return false;
    }
    return true;
}

void MatchPlanner::variable(const NormalizedPattern &pattern, const std::size_t input) {
    const auto &binding = *bindings.at(&module.syntax->expression(pattern.expression));
    const auto operation = binding.use == BindingUse::definition && definitions.insert(binding.identity).second
                               ? MatchOperation::bind
                               : MatchOperation::exact_binding;
    if (skip && operation == MatchOperation::exact_binding) {
        return;
    }
    plan.nodes.push_back({pattern.origin, operation, input, binding.identity});
    if (operation == MatchOperation::bind) {
        plan.outputs.push_back(binding.identity);
    }
}

bool MatchPlanner::node(const NormalizedPattern &pattern, const std::size_t input) {
    if (pattern.kind == PatternKind::wildcard || pattern.kind == PatternKind::alias) {
        return true;
    }
    if (pattern.kind == PatternKind::variable) {
        variable(pattern, input);
        return true;
    }
    if (const auto value = literal(pattern, bits)) {
        plan.nodes.push_back({pattern.origin, MatchOperation::exact_literal, input, {}, value});
        return true;
    }
    reject_capability(module, module.syntax->expression(pattern.origin).source, "pattern matching", out);
    return false;
}

namespace {
using Planner = MatchPlanner;

// Indexing is budgeted too; no unchecked syntax walk or lookup precedes normalized pattern analysis.
bool index(Planner &state, const Function &function, const ast::ExprId &site) {
    for (const auto &pattern : function.patterns) {
        if (!state.spend(site)) {
            return false;
        }
        state.patterns.emplace(&state.module.syntax->expression(pattern.origin), &pattern);
        state.patterns.emplace(&state.module.syntax->expression(pattern.expression), &pattern);
    }
    for (const auto &binding : function.bindings) {
        if (!state.spend(site)) {
            return false;
        }
        state.bindings.emplace(&state.module.syntax->expression(binding.expression), &binding);
    }
    return true;
}

// Containers schedule checked extraction before their children; aliases reuse their original input.
bool visit_pattern(Planner &state, const PatternVisit &visit, std::vector<MatchTask> &pending) {
    if (!state.spend(visit.id)) {
        return false;
    }
    const auto &pattern = *state.patterns.at(&state.module.syntax->expression(visit.id));
    if (container_pattern(pattern)) {
        return expand_container(state, visit, pattern, pending);
    }
    if (!state.node(pattern, visit.input)) {
        return false;
    }
    if (pattern.kind == PatternKind::alias) {
        for (auto child = pattern.children.rbegin(); child != pattern.children.rend(); ++child) {
            pending.emplace_back(PatternVisit{*child, visit.input});
        }
    }
    return true;
}

// Planned operations and source visits share a bounded explicit stack.
bool task(Planner &state, const MatchTask &task, std::vector<MatchTask> &pending) {
    if (const auto *node = std::get_if<MatchNode>(&task)) {
        state.plan.nodes.push_back(*node);
        return true;
    }
    return visit_pattern(state, std::get<PatternVisit>(task), pending);
}

// Expand nested patterns without consuming the native C++ call stack.
bool argument(Planner &state, const ast::ExprId &root, const std::size_t input) {
    std::vector<MatchTask> pending{PatternVisit{root, input}};
    while (!pending.empty()) {
        auto next = std::move(pending.back());
        pending.pop_back();
        if (!task(state, next, pending)) {
            return false;
        }
    }
    return true;
}

// Explicit continuations make the same plan usable by later clauses and body-match callers.
void finish(MatchPlan &plan, const ast::ExprId &site) {
    const auto success = plan.nodes.size();
    const auto mismatch = success + 1;
    for (std::size_t i = 0; i < success; ++i) {
        plan.nodes[i].success = i + 1;
        plan.nodes[i].mismatch = mismatch;
    }
    plan.nodes.push_back({site, MatchOperation::success, 0});
    plan.nodes.push_back({site, MatchOperation::mismatch, 0});
}

// Share bounded normalization consumption between function heads and body matches.
std::optional<MatchPlan> build_plan(const Module &module, const Function &function,
                                    const std::span<const ast::ExprId> roots, const ast::ExprId &site,
                                    const Reporter &out, MatchOptions options) {
    const auto limit = options.work_limit;
    Planner state{module, out, options.word_bits, {}, {}, {roots.size(), {}, {}, roots.size()}, 0, limit};
    if (options.generator != GeneratorPattern::none) {
        state.generator = &module.syntax->expression(roots.front());
        state.skip = options.generator == GeneratorPattern::skip;
    }
    if (!index(state, function, site)) {
        return {};
    }
    for (std::size_t input = 0; input < roots.size(); ++input) {
        if (!argument(state, roots[input], input)) {
            return {};
        }
    }
    if (limit - std::min(limit, state.work) < 2 || !state.spend(site) || !state.spend(site)) {
        report(module, &module.syntax->expression(site).source, "match plan node limit exceeded", out);
        return {};
    }
    finish(state.plan, site);
    return std::move(state.plan);
}
} // namespace

std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function,
                                         const ast::FunctionClause &clause, const Reporter &out, MatchOptions options) {
    std::vector<ast::ExprId> roots;
    roots.reserve(clause.arguments.size());
    for (const auto &argument : clause.arguments) {
        roots.push_back(pattern_root(*module.syntax, argument));
    }
    return build_plan(module, function, roots, clause.body.at(0), out, options);
}

std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const Reporter &out,
                                         MatchOptions options) {
    const auto &clause = std::get<ast::Function>(module.syntax->form(function.form).value).clauses.at(options.clause);
    return make_match_plan(module, function, clause, out, options);
}

std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const ast::ExprId &pattern,
                                         const Reporter &out, MatchOptions options) {
    return build_plan(module, function, std::array{pattern}, pattern, out, options);
}
} // namespace erlang_aot::semantic
