#include "match_plan.hpp"
#include "capabilities.hpp"
#include "features.hpp"
#include <charconv>
#include <erlang_aot/abi/term.hpp>

namespace erlang_aot::semantic {
namespace {
// Decode owned folded decimal text without arbitrary-size narrowing or host-width assumptions.
std::optional<MatchLiteral> integer(const ast::IntegerLiteral &integer, unsigned bits) {
    std::int64_t number = 0;
    const auto &text = integer.value.decimal;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return {};
    }
    const bool fits = bits == 32 ? abi::v1::IntegerEncoding<32>::encode(number).has_value()
                                 : bits == 64 && abi::v1::IntegerEncoding<64>::encode(number).has_value();
    return fits ? std::optional<MatchLiteral>{number} : std::nullopt;
}

// Canonical empty patterns need no extraction; later nonempty containers stay capability-gated.
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

// Folded pattern integers must fit the selected layout before a plan literal can be encoded.
std::optional<MatchLiteral> literal(const NormalizedPattern &pattern, unsigned bits) {
    if (const auto value = empty(pattern)) {
        return value;
    }
    if (!pattern.literal) {
        return {};
    }
    if (const auto *atom = std::get_if<ast::Atom>(&*pattern.literal)) {
        return *atom;
    }
    const auto *integer = std::get_if<ast::IntegerLiteral>(&*pattern.literal);
    return integer ? semantic::integer(*integer, bits) : std::nullopt;
}

struct Planner {
    // Index immutable normalized nodes/events once, avoiding repeated scans for wide patterns.
    const Module &module;
    const Reporter &out;
    unsigned bits;
    std::map<const ast::Expression *, const NormalizedPattern *> patterns;
    std::map<const ast::Expression *, const Binding *> bindings;
    MatchPlan plan;
    std::size_t work = 0;
    std::size_t limit;

    // No partially built plan escapes resource exhaustion.
    bool spend(const ast::ExprId &site) {
        if (work++ >= limit) {
            report(module, &module.syntax->expression(site).source, "match plan work limit exceeded", out);
            return false;
        }
        return true;
    }

    // First definitions publish a tentative slot; repeated occurrences test exactly that slot.
    void variable(const NormalizedPattern &pattern, std::size_t input) {
        const auto &binding = *bindings.at(&module.syntax->expression(pattern.expression));
        const auto operation =
            binding.use == BindingUse::definition ? MatchOperation::bind : MatchOperation::exact_binding;
        plan.nodes.push_back({pattern.origin, operation, input, binding.identity});
        if (operation == MatchOperation::bind) {
            plan.outputs.push_back(binding.identity);
        }
    }

    // Aliases are scheduled by the caller on the same input, never evaluated as assignments.
    bool node(const NormalizedPattern &pattern, std::size_t input) {
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
};

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

// Expand compound patterns iteratively, sharing one candidate input across every alias operand.
bool argument(Planner &state, const ast::ExprId &root, std::size_t input) {
    std::vector<ast::ExprId> pending{root};
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!state.spend(id)) {
            return false;
        }
        const auto &pattern = *state.patterns.at(&state.module.syntax->expression(id));
        if (!state.node(pattern, input)) {
            return false;
        }
        if (pattern.kind == PatternKind::alias) {
            pending.insert(pending.end(), pattern.children.rbegin(), pattern.children.rend());
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
} // namespace

std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const Reporter &out,
                                         MatchOptions options) {
    const auto &syntax = std::get<ast::Function>(module.syntax->form(function.form).value).clauses.at(options.clause);
    const auto site = syntax.body.at(0);
    const auto limit = options.work_limit;
    Planner state{module, out, options.word_bits, {}, {}, {syntax.arguments.size(), {}, {}}, 0, limit};
    if (!index(state, function, site)) {
        return {};
    }
    for (std::size_t input = 0; input < syntax.arguments.size(); ++input) {
        const auto root = std::visit([](const auto &p) { return p.expression; },
                                     module.syntax->pattern(syntax.arguments[input]).value);
        if (!argument(state, root, input)) {
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
} // namespace erlang_aot::semantic
