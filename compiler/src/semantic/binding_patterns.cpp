#include "binding_state.hpp"

namespace erlang_aot::semantic {
namespace {
// Ordinary children retain source evaluation order; branch/closure scopes are deliberately opaque here.
struct Children {
    template <typename T> std::vector<ast::ExprId> operator()(const T &) const { return {}; }

    std::vector<ast::ExprId> operator()(const ast::Group &v) const { return {v.expression}; }

    std::vector<ast::ExprId> operator()(const ast::UnaryExpression &v) const { return {v.operand}; }

    std::vector<ast::ExprId> operator()(const ast::BinaryExpression &v) const { return {v.left, v.right}; }

    std::vector<ast::ExprId> operator()(const ast::MatchExpression &v) const { return {v.left, v.right}; }

    std::vector<ast::ExprId> operator()(const ast::Tuple &v) const { return v.elements; }

    std::vector<ast::ExprId> operator()(const ast::BlockExpression &v) const { return v.body; }

    std::vector<ast::ExprId> operator()(const ast::RemoteExpression &v) const { return {v.module, v.function}; }

    std::vector<ast::ExprId> operator()(const ast::RecordAccess &v) const { return {v.base}; }

    std::vector<ast::ExprId> operator()(const ast::List &v) const {
        auto result = v.elements;
        if (v.tail) {
            result.push_back(*v.tail);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::CallExpression &v) const {
        std::vector<ast::ExprId> result{v.target};
        result.insert(result.end(), v.arguments.begin(), v.arguments.end());
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::MapExpression &v) const {
        std::vector<ast::ExprId> result;
        if (v.base) {
            result.push_back(*v.base);
        }
        for (const auto &field : v.fields) {
            result.push_back(field.key);
            result.push_back(field.value);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::RecordExpression &v) const {
        std::vector<ast::ExprId> result;
        if (v.base) {
            result.push_back(*v.base);
        }
        for (const auto &field : v.fields) {
            result.push_back(field.value);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::Bitstring &v) const {
        std::vector<ast::ExprId> result;
        for (const auto &segment : v.segments) {
            result.push_back(segment.value);
            if (segment.size) {
                result.push_back(*segment.size);
            }
        }
        return result;
    }
};

struct PatternVisit {
    // Preserve whole-argument provenance only through parentheses and aliases, never extraction.
    ast::ExprId id;
    std::optional<std::size_t> argument;
    bool read = false;
};

// Map keys read existing names; their values retain the enclosing binding context.
std::vector<PatternVisit> map_children(const ast::MapExpression &map, const bool read) {
    std::vector<PatternVisit> result;
    if (map.base) {
        result.push_back({*map.base, {}, true});
    }
    for (const auto &field : map.fields) {
        result.push_back({field.key, {}, true});
        result.push_back({field.value, {}, read});
    }
    return result;
}

// Segment sizes are read before defining the segment value; full legality belongs to step 5.
std::vector<PatternVisit> binary_children(const ast::Bitstring &binary, const bool read) {
    std::vector<PatternVisit> result;
    for (const auto &segment : binary.segments) {
        if (segment.size) {
            result.push_back({*segment.size, {}, true});
        }
        result.push_back({segment.value, {}, read});
    }
    return result;
}

// Keys and sizes are reads, not declarations; context-specific legality is normalized in step 5.
std::vector<PatternVisit> pattern_children(const ast::ExprValue &value, const PatternVisit &visit) {
    if (const auto *map = std::get_if<ast::MapExpression>(&value)) {
        return map_children(*map, visit.read);
    }
    if (const auto *binary = std::get_if<ast::Bitstring>(&value)) {
        return binary_children(*binary, visit.read);
    }
    const auto argument =
        std::holds_alternative<ast::Group>(value) || std::holds_alternative<ast::MatchExpression>(value)
            ? visit.argument
            : std::nullopt;
    std::vector<PatternVisit> result;
    for (const auto &child : binding_children(value)) {
        result.push_back({child, argument, visit.read});
    }
    return result;
}
} // namespace

std::vector<ast::ExprId> binding_children(const ast::ExprValue &value) { return std::visit(Children{}, value); }

void bind_pattern(BindingAnalysis &state, const ast::ExprId &id, BindingCandidate &scope, const BindingContext context,
                  const std::optional<std::size_t> argument) {
    std::vector<PatternVisit> pending{{id, argument}};
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id)) {
            scope.valid = false;
            return;
        }
        const auto &value = state.module.syntax->expression(visit.id).value;
        if (visit.read) {
            state.read(visit.id, scope, context);
        } else if (std::holds_alternative<ast::Variable>(value)) {
            state.define(visit.id, scope, context, visit.argument);
        }
        const auto children = pattern_children(value, visit);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
}
} // namespace erlang_aot::semantic
