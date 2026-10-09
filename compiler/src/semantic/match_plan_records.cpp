#include "features.hpp"
#include "match_plan_internal.hpp"
#include "records.hpp"
#include <clause/abi/records.hpp>

namespace clause::semantic {
namespace {
// The identity a native pattern tests: its module, name and check.
struct NativeIdentity {
    ast::Atom module;
    ast::Atom name;
    abi::v1::RecordCheck check;
};

// A native pattern tests the record's identity, then extracts each listed field in source order; a field the
// record lacks fails the match.
bool expand_native(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                   const ast::RecordExpression &record, const NativeIdentity &identity,
                   std::vector<MatchTask> &pending) {
    std::vector<const ast::RecordField *> fields;
    fields.reserve(record.fields.size());
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
    MatchNode test{pattern.origin, MatchOperation::record_test, visit.input, {}, identity.name};
    test.index = static_cast<std::size_t>(identity.check);
    test.record_module = identity.module;
    pending.emplace_back(test);
    return true;
}

// What a native pattern tests: a qualified or imported record needs an export only when it names a field; a
// local one tests this module.
std::optional<NativeIdentity> native_identity(const Module &module, const ast::RecordExpression &record) {
    if (anonymous_record(record.identity)) {
        // #_{} matches any native record; listing a field needs it exported or defined in this module. The
        // name operand is unused, so the module's name fills it.
        const auto check = record.fields.empty() ? abi::v1::RecordCheck::any : abi::v1::RecordCheck::exported_or_module;
        return NativeIdentity{{module.name}, {module.name}, check};
    }
    if (const auto external = external_record(module, record.identity)) {
        const auto check =
            record.fields.empty() ? abi::v1::RecordCheck::module_name : abi::v1::RecordCheck::exported_module_name;
        return NativeIdentity{{external->module}, {external->name}, check};
    }
    const auto *layout = record_layout(module, record.identity);
    if (layout && layout->native) {
        return NativeIdentity{{module.name}, layout->name, abi::v1::RecordCheck::module_name};
    }
    return std::nullopt;
}

// A tuple record pattern checks arity and tag, then extracts its listed fields by position.
bool expand_tuple(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                  const ast::RecordExpression &record, const RecordLayout &layout, std::vector<MatchTask> &pending) {
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
        const auto &value = fields[i - 1];
        if (!value) {
            continue;
        }
        pending.emplace_back(PatternVisit{*value, --output});
        MatchNode field{pattern.origin, MatchOperation::tuple_element, visit.input};
        field.output = output;
        field.index = i;
        pending.emplace_back(field);
    }
    pending.emplace_back(MatchNode{pattern.origin, MatchOperation::exact_literal, base + count, {}, layout.name});
    MatchNode tag{pattern.origin, MatchOperation::tuple_element, visit.input};
    tag.output = base + count;
    tag.index = 0;
    pending.emplace_back(tag);
    MatchNode shape{pattern.origin, MatchOperation::tuple_shape, visit.input};
    shape.index = fields.size() + 1;
    pending.emplace_back(shape);
    return true;
}
} // namespace

bool expand_record(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                   std::vector<MatchTask> &pending) {
    const auto &record = std::get<ast::RecordExpression>(state.module.syntax->expression(pattern.expression).value);
    if (const auto identity = native_identity(state.module, record)) {
        return expand_native(state, visit, pattern, record, *identity, pending);
    }
    const auto *layout = record_layout(state.module, record.identity);
    if (!layout) {
        reject_capability(state.module, state.module.syntax->expression(pattern.origin).source, "heap expressions",
                          state.out);
        return false;
    }
    return expand_tuple(state, visit, pattern, record, *layout, pending);
}
} // namespace clause::semantic
