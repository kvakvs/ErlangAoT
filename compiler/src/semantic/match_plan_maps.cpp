#include "match_plan_internal.hpp"

namespace erlang_aot::semantic {
bool expand_map(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                std::vector<MatchTask> &pending) {
    const auto &map = std::get<ast::MapExpression>(state.module.syntax->expression(pattern.expression).value);
    if (!state.spend(pattern.origin)) {
        return false;
    }
    for (auto field = map.fields.rbegin(); field != map.fields.rend(); ++field) {
        if (!state.spend(field->key) || !state.spend(field->value)) {
            return false;
        }
        const auto slot = state.plan.values++;
        pending.emplace_back(PatternVisit{field->value, slot});
        MatchNode lookup{pattern.origin, MatchOperation::map_lookup, visit.input};
        lookup.key = field->key;
        lookup.output = slot;
        pending.emplace_back(lookup);
    }
    pending.emplace_back(MatchNode{pattern.origin, MatchOperation::map_shape, visit.input});
    return true;
}
} // namespace erlang_aot::semantic
