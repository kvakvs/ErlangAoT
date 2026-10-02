#include "features.hpp"
#include "match_plan_internal.hpp"
#include "records.hpp"

namespace erlang_aot::semantic {
bool expand_record(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                   std::vector<MatchTask> &pending) {
    const auto &record = std::get<ast::RecordExpression>(state.module.syntax->expression(pattern.expression).value);
    const auto *layout = record_layout(state.module, record.identity);
    if (!layout || layout->native) {
        reject_capability(state.module, state.module.syntax->expression(pattern.origin).source, "heap expressions",
                          state.out);
        return false;
    }
    const auto fields = record_values(state.module, record, true);
    const auto count = pattern.children.size();
    for (std::size_t i = 0; i < 3 + 2 * fields.size(); ++i) {
        if (!state.spend(visit.id)) {
            return false;
        }
    }
    const auto base = state.plan.values;
    state.plan.values += count + 1;
    auto output = base + count;
    for (std::size_t i = fields.size(); i != 0; --i) {
        if (!fields[i - 1]) {
            continue;
        }
        pending.emplace_back(PatternVisit{*fields[i - 1], --output});
        MatchNode field{pattern.origin, MatchOperation::tuple_element, visit.input};
        field.output = output;
        field.index = i;
        pending.emplace_back(field);
    }
    pending.emplace_back(MatchNode{pattern.origin, MatchOperation::exact_literal, base + count, {}, layout->name});
    MatchNode tag{pattern.origin, MatchOperation::tuple_element, visit.input};
    tag.output = base + count;
    tag.index = 0;
    pending.emplace_back(tag);
    MatchNode shape{pattern.origin, MatchOperation::tuple_shape, visit.input};
    shape.index = fields.size() + 1;
    pending.emplace_back(shape);
    return true;
}
} // namespace erlang_aot::semantic
