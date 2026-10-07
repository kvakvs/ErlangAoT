#include "features.hpp"
#include "match_plan_internal.hpp"
#include "records.hpp"
#include <erlang_aot/abi/records.hpp>

namespace erlang_aot::semantic {
namespace {
// A local native pattern tests module and name, then extracts each listed field in source order; a field the
// record lacks fails the match.
bool expand_native(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                   const ast::RecordExpression &record, const RecordLayout &layout, std::vector<MatchTask> &pending) {
    std::vector<const ast::RecordField *> fields;
    for (const auto &field : record.fields) {
        if (std::holds_alternative<ast::Atom>(field.name)) {
            fields.push_back(&field);
        }
    }
    for (std::size_t i = 0; i < 1 + 2 * fields.size(); ++i) {
        if (!state.spend(visit.id)) {
            return false;
        }
    }
    const auto base = state.plan.values;
    state.plan.values += fields.size();
    for (std::size_t i = fields.size(); i != 0; --i) {
        pending.emplace_back(PatternVisit{fields[i - 1]->value, base + i - 1});
        MatchNode field{
            pattern.origin, MatchOperation::record_field, visit.input, {}, std::get<ast::Atom>(fields[i - 1]->name)};
        field.output = base + i - 1;
        pending.emplace_back(field);
    }
    MatchNode test{pattern.origin, MatchOperation::record_test, visit.input, {}, layout.name};
    test.index = static_cast<std::size_t>(abi::v1::RecordCheck::module_name);
    test.record_module = ast::Atom{state.module.name};
    pending.emplace_back(test);
    return true;
}
} // namespace

bool expand_record(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                   std::vector<MatchTask> &pending) {
    const auto &record = std::get<ast::RecordExpression>(state.module.syntax->expression(pattern.expression).value);
    const auto *layout = record_layout(state.module, record.identity);
    if (!layout) {
        reject_capability(state.module, state.module.syntax->expression(pattern.origin).source, "heap expressions",
                          state.out);
        return false;
    }
    if (layout->native) {
        return expand_native(state, visit, pattern, record, *layout, pending);
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
