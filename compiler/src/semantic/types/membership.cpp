#include "contracts.hpp"
#include <algorithm>
#include <array>
#include <set>

namespace clause::semantic::types {
namespace {
// Compare canonical decimal integers exactly, including bounds larger than a host word.
bool less(std::string_view left, std::string_view right) {
    const bool negative = left.starts_with('-');
    if (negative != right.starts_with('-')) {
        return negative;
    }
    if (negative) {
        left.remove_prefix(1);
        right.remove_prefix(1);
    }
    const auto order = left.size() == right.size() ? left.compare(right) : (left.size() < right.size() ? -1 : 1);
    return negative ? order > 0 : order < 0;
}

// Known numeric builtins admit exact integers without claiming a machine representation.
bool builtin_excludes(const Node &node, const std::string_view value) {
    constexpr std::array general{"integer", "number"};
    if (std::ranges::contains(general, node.name)) {
        return false;
    }
    if (node.name == "pos_integer") {
        return !less("0", value);
    }
    if (node.name == "neg_integer") {
        return !less(value, "0");
    }
    if (node.name == "non_neg_integer" || node.name == "timeout") {
        return less(value, "0");
    }
    const std::map<std::string_view, std::string_view> bounded{{"byte", "255"}, {"arity", "255"}, {"char", "1114111"}};
    if (const auto limit = bounded.find(node.name); limit != bounded.end()) {
        return less(value, "0") || less(limit->second, value);
    }
    return true;
}

// Flat structural categories and integer ranges can exclude a singleton without recursive analysis.
bool concrete_excludes(const Graph &graph, const Node &node, const std::string_view value) {
    if (node.kind == Kind::integer) {
        return node.name != value;
    }
    if (node.kind == Kind::range) {
        return less(value, graph.get(node.children.at(0)).name) || less(graph.get(node.children.at(1)).name, value);
    }
    if (node.kind == Kind::application) {
        return builtin_excludes(node, value);
    }
    constexpr std::array disjoint{Kind::bottom, Kind::atom,   Kind::tuple,     Kind::list,
                                  Kind::map,    Kind::record, Kind::bitstring, Kind::function};
    return std::ranges::contains(disjoint, node.kind);
}

// Stop on hidden or repeated references; recursive contracts cannot establish exclusion.
bool expand(Registry &declared, const Id id, const std::string_view owner, std::vector<Id> &pending,
            std::set<Id> &references) {
    if (!references.insert(id).second) {
        return false;
    }
    const auto expanded = expand_reference(declared, id, owner);
    if (!expanded) {
        return false;
    }
    pending.push_back(*expanded);
    return true;
}
} // namespace

bool excludes_integer(Registry &declared, const std::string_view value, const Id type, const std::string_view owner) {
    std::vector<Id> pending{type};
    std::set<Id> references;
    std::size_t work = 0;
    while (!pending.empty()) {
        if (++work > declared.graph.limits().syntax_work) {
            return false;
        }
        const auto id = pending.back();
        pending.pop_back();
        const auto node = declared.graph.get(id);
        if (node.kind == Kind::union_type) {
            pending.insert(pending.end(), node.children.begin(), node.children.end());
        } else if (node.kind == Kind::reference) {
            if (!expand(declared, id, owner, pending, references)) {
                return false;
            }
        } else if (!concrete_excludes(declared.graph, node, value)) {
            return false;
        }
    }
    return true;
}
} // namespace clause::semantic::types
