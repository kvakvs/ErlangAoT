#include "match_plan_internal.hpp"

namespace erlang_aot::semantic {
namespace {
// Charge every scheduled operation before growing either the task stack or the candidate-slot table.
bool charge(MatchPlanner &state, const ast::ExprId &site, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (!state.spend(site)) {
            return false;
        }
    }
    return true;
}

// Tuple shape dominates all zero-based field extraction and recursively scheduled constraints.
bool tuple(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
           std::vector<MatchTask> &pending) {
    if (!charge(state, visit.id, 1 + 2 * pattern.children.size())) {
        return false;
    }
    const auto base = state.plan.values;
    state.plan.values += pattern.children.size();
    for (std::size_t i = pattern.children.size(); i != 0; --i) {
        pending.emplace_back(PatternVisit{pattern.children[i - 1], base + i - 1});
        MatchNode field{pattern.origin, MatchOperation::tuple_element, visit.input};
        field.output = base + i - 1;
        field.index = i - 1;
        pending.emplace_back(field);
    }
    MatchNode shape{pattern.origin, MatchOperation::tuple_shape, visit.input};
    shape.index = pattern.children.size();
    pending.emplace_back(shape);
    return true;
}

struct Chain {
    // Borrow either list syntax or decoded string characters without duplicating large literal storage.
    const NormalizedPattern &pattern;
    const ast::List *list;
    const ast::StringLiteral *string;
    std::size_t base;
};

// Prefix concatenation replaces only the literal prefix's terminal nil constraint with its suffix pattern.
void terminus(const Chain &chain, const PatternVisit &visit, std::vector<MatchTask> &pending) {
    if (chain.list && chain.list->tail) {
        pending.emplace_back(PatternVisit{*chain.list->tail, visit.input, visit.suffix});
    } else if (visit.suffix) {
        pending.emplace_back(PatternVisit{*visit.suffix, visit.input});
    } else {
        pending.emplace_back(
            MatchNode{chain.pattern.origin, MatchOperation::exact_literal, visit.input, {}, EmptyValue::list});
    }
}

// Each cons is checked before accessing its head; the tail becomes the next candidate only after success.
void cell(const Chain &chain, const PatternVisit &visit, std::size_t index, std::vector<MatchTask> &pending) {
    const auto head = chain.base + 2 * index;
    MatchNode tail{chain.pattern.origin, MatchOperation::cons_tail, visit.input};
    tail.output = head + 1;
    pending.emplace_back(tail);
    if (chain.list) {
        pending.emplace_back(PatternVisit{chain.list->elements[index], head});
    } else {
        pending.emplace_back(MatchNode{chain.pattern.origin,
                                       MatchOperation::exact_literal,
                                       head,
                                       {},
                                       static_cast<std::int64_t>(chain.string->value[index])});
    }
    MatchNode field{chain.pattern.origin, MatchOperation::cons_head, visit.input};
    field.output = head;
    pending.emplace_back(field);
    pending.emplace_back(MatchNode{chain.pattern.origin, MatchOperation::cons_shape, visit.input});
}

// Expand list/string spines iteratively, retaining an explicit final proper or improper tail constraint.
bool chain(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
           std::vector<MatchTask> &pending) {
    const auto &expression = state.module.syntax->expression(pattern.expression).value;
    const auto *list = std::get_if<ast::List>(&expression);
    const auto *string = pattern.literal ? std::get_if<ast::StringLiteral>(&*pattern.literal) : nullptr;
    const auto count = list ? list->elements.size() : string->value.size();
    if (!charge(state, visit.id, 1 + 4 * count)) {
        return false;
    }
    const Chain chain{pattern, list, string, state.plan.values};
    state.plan.values += 2 * count;
    terminus(chain, {visit.id, count == 0 ? visit.input : state.plan.values - 1, visit.suffix}, pending);
    for (std::size_t i = count; i != 0; --i) {
        cell(chain, {visit.id, i == 1 ? visit.input : chain.base + 2 * i - 3}, i - 1, pending);
    }
    return true;
}
} // namespace

bool container_pattern(const NormalizedPattern &pattern) {
    return pattern.kind == PatternKind::bitstring || pattern.kind == PatternKind::map ||
           pattern.kind == PatternKind::tuple || pattern.kind == PatternKind::list ||
           pattern.kind == PatternKind::prefix ||
           (pattern.literal && std::holds_alternative<ast::StringLiteral>(*pattern.literal));
}

bool expand_container(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                      std::vector<MatchTask> &pending) {
    if (pattern.kind == PatternKind::bitstring) {
        return expand_bits(state, visit, pattern, pending);
    }
    if (pattern.kind == PatternKind::map) {
        return expand_map(state, visit, pattern, pending);
    }
    if (pattern.kind == PatternKind::tuple) {
        return tuple(state, visit, pattern, pending);
    }
    if (pattern.kind == PatternKind::prefix) {
        if (!state.spend(visit.id)) {
            return false;
        }
        pending.emplace_back(PatternVisit{pattern.children.at(0), visit.input, pattern.children.at(1)});
        return true;
    }
    return chain(state, visit, pattern, pending);
}
} // namespace erlang_aot::semantic
