#include "type_format.hpp"
#include "display.hpp"
#include <algorithm>
#include <array>
#include <span>

namespace erlang_aot::cli {
namespace {
namespace types = semantic::types;

struct DisplayBudget {
    // Bound repeated graph traversal and nesting without changing semantic inference precision.
    std::size_t work = 1024;
};

// Keep ordinary Erlang identifiers readable; unusual names use escaped quoted display text.
bool identifier_character(unsigned char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
           value == '_' || value == '@';
}

// Quote names that contain punctuation/control bytes so they cannot impersonate report structure.
std::string name_text(std::string_view name) {
    return !name.empty() && std::ranges::all_of(name, identifier_character) ? std::string(name) : quote_text(name);
}

// These leaves retain exact singleton values; top and bottom have distinct conventional names.
std::string primitive(const types::Node &node) {
    switch (node.kind) {
    case types::Kind::top:
        return "term()";
    case types::Kind::bottom:
        return "none()";
    case types::Kind::integer:
        return node.name;
    case types::Kind::atom:
        return "atom(" + quote_text(node.name) + ")";
    case types::Kind::variable:
        return "variable(" + quote_text(node.name) + ")";
    default:
        return {};
    }
}

// Preserve structural kind and identity even for categories outside executable language support.
std::string head(const types::Node &node) {
    constexpr std::array kinds{"top",         "bottom", "atom",   "integer",    "variable", "range",
                               "application", "tuple",  "list",   "map",        "record",   "bitstring",
                               "fun",         "unary",  "binary", "annotation", "union",    "reference"};
    if (node.kind == types::Kind::application || node.kind == types::Kind::reference) {
        return node.module.empty() ? name_text(node.name) : name_text(node.module) + ':' + name_text(node.name);
    }
    auto result = std::string(kinds.at(static_cast<std::size_t>(node.kind)));
    if (!node.module.empty()) {
        result += ':' + name_text(node.module);
    }
    if (!node.name.empty()) {
        result += ':' + name_text(node.name);
    }
    return result;
}

// Walk only bounded child references; recursive declarations stay symbolic references rather than expanding bodies.
std::string describe(const types::Graph &graph, types::Id id, std::size_t depth, DisplayBudget &budget);

// Stop a wide product once its shared display budget is exhausted.
std::string children(const types::Graph &graph, std::span<const types::Id> ids, std::size_t depth,
                     DisplayBudget &budget) {
    std::string result;
    for (const auto id : ids) {
        if (!result.empty()) {
            result += ", ";
        }
        if (budget.work == 0) {
            result += "<display-limit>";
            break;
        }
        result += describe(graph, id, depth + 1, budget);
    }
    return result;
}

// Render function products with an explicit result position instead of treating specifications as inferred facts.
std::string function(const types::Graph &graph, const types::Node &node, std::size_t depth, DisplayBudget &budget) {
    if (node.children.empty()) {
        return "fun()";
    }
    const auto arguments = std::span(node.children).first(node.children.size() - 1);
    const auto input = node.name == "any_arguments" ? "..." : children(graph, arguments, depth, budget);
    return "fun((" + input + ") -> " + describe(graph, node.children.back(), depth + 1, budget) + ')';
}

// Retain structural roles (map exactness, record fields, bitstring units) in deterministic order.
std::string labels(const types::Node &node) {
    if (node.kind == types::Kind::reference) {
        return {};
    }
    std::string result;
    for (const auto &label : node.labels) {
        result += result.empty() ? " labels=[" : ", ";
        result += quote_text(label);
    }
    return result.empty() ? result : result + ']';
}

std::string describe(const types::Graph &graph, types::Id id, std::size_t depth, DisplayBudget &budget) {
    if (depth >= 32 || budget.work == 0) {
        return "<display-limit>";
    }
    --budget.work;
    const auto &node = graph.get(id);
    if (auto leaf = primitive(node); !leaf.empty()) {
        return leaf;
    }
    if (node.kind == types::Kind::function) {
        return function(graph, node, depth, budget);
    }
    return head(node) + '(' + children(graph, node.children, depth, budget) + ')' + labels(node);
}
} // namespace

std::string type_text(const semantic::types::Graph &graph, semantic::types::Id type) {
    DisplayBudget budget;
    return describe(graph, type, 0, budget);
}

std::string fact_text(const semantic::types::Inference &inferred, semantic::types::Fact fact) {
    auto result = type_text(inferred.graph, fact.type);
    if (inferred.graph.get(fact.type).kind == semantic::types::Kind::top) {
        result += " [unknown]";
    }
    if (fact.argument) {
        result += " [argument[" + std::to_string(*fact.argument) + "] relation]";
    }
    return result;
}
} // namespace erlang_aot::cli
