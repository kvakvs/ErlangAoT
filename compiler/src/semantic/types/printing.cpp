#include "printing.hpp"
#include <algorithm>
#include <clause/compiler/printing.hpp>
#include <span>

namespace clause::semantic::types {
namespace {
// Deeper nesting than this prints as `...`, like an exhausted budget.
constexpr std::size_t MAX_DEPTH = 32;

class Printer final {
  public:
    Printer(const Graph &graph, std::size_t budget) : graph_(graph), budget_(budget) {}

    // The text of one node and, within the budget, its children.
    std::string text(Id id, std::size_t depth) {
        if (budget_ == 0 || depth >= MAX_DEPTH) {
            return "...";
        }
        --budget_;
        const auto &node = graph_.get(id);
        if (auto leaf = scalar(node); !leaf.empty()) {
            return leaf;
        }
        return compound(node, depth + 1);
    }

  private:
    // Leaves: term(), none(), atoms, integers and type variables.
    static std::string scalar(const Node &node) {
        switch (node.kind) {
        case Kind::top:
            return "term()";
        case Kind::bottom:
            return "none()";
        case Kind::atom:
            return atom_source(node.name);
        case Kind::integer:
        case Kind::variable:
            return node.name;
        default:
            return {};
        }
    }

    // Children separated by `separator`.
    std::string join(std::span<const Id> children, std::size_t depth, std::string_view separator = ", ") {
        std::string result;
        for (const auto child : children) {
            result += (result.empty() ? "" : std::string(separator)) + text(child, depth);
        }
        return result;
    }

    // A named type with arguments; predefined erlang types drop their module.
    std::string named(const Node &node, std::size_t depth) {
        const auto prefix =
            node.module.empty() || node.module == "erlang" ? std::string() : atom_source(node.module) + ':';
        return prefix + atom_source(node.name) + '(' + join(node.children, depth) + ')';
    }

    std::string list(const Node &node, std::size_t depth) {
        if (node.children.empty()) {
            return "[]";
        }
        return '[' + text(node.children.front(), depth) + (node.name == "nonempty" ? ", ...]" : "]");
    }

    // Map fields alternate keys and values; each field's label is its operator (0: =>, 1: :=).
    std::string map(const Node &node, std::size_t depth) {
        if (node.name == "any") {
            return "map()";
        }
        std::string fields;
        for (std::size_t i = 0; i + 1 < node.children.size(); i += 2) {
            const bool exact = i / 2 < node.labels.size() && node.labels[i / 2] == "1";
            fields += (fields.empty() ? "" : ", ") + text(node.children[i], depth) + (exact ? " := " : " => ") +
                      text(node.children[i + 1], depth);
        }
        return "#{" + fields + '}';
    }

    // Record fields pair each label with its type.
    std::string record(const Node &node, std::size_t depth) {
        std::string fields;
        for (std::size_t i = 0; i < node.children.size() && i < node.labels.size(); ++i) {
            fields +=
                (fields.empty() ? "" : ", ") + atom_source(node.labels[i]) + " :: " + text(node.children[i], depth);
        }
        const auto module = node.module.empty() ? std::string() : atom_source(node.module) + ':';
        return '#' + module + atom_source(node.name) + '{' + fields + '}';
    }

    // <<_:Base, _:_*Unit>> with the parts its labels name.
    std::string bitstring(const Node &node, std::size_t depth) {
        std::string parts;
        for (std::size_t i = 0; i < node.children.size() && i < node.labels.size(); ++i) {
            const auto size = text(node.children[i], depth);
            parts += (parts.empty() ? "" : ", ") + (node.labels[i] == "unit" ? "_:_*" + size : "_:" + size);
        }
        return "<<" + parts + ">>";
    }

    std::string function(const Node &node, std::size_t depth) {
        const bool result = std::ranges::contains(node.labels, std::string("result"));
        if (!result) {
            return "fun()";
        }
        const auto arguments = std::span(node.children).first(node.children.size() - 1);
        const auto input = node.name == "any_arguments" ? std::string("...") : join(arguments, depth);
        return "fun((" + input + ") -> " + text(node.children.back(), depth) + ')';
    }

    // Unary and binary type operators carry their operator code as the node's name.
    std::string operation(const Node &node, std::size_t depth) {
        const auto code = static_cast<std::uint8_t>(std::stoul(node.name));
        if (node.kind == Kind::unary) {
            return operator_source(static_cast<ast::UnaryOperator>(code)) + ' ' + text(node.children.front(), depth);
        }
        return text(node.children.front(), depth) + ' ' + operator_source(static_cast<ast::BinaryOperator>(code)) +
               ' ' + text(node.children.back(), depth);
    }

    // An annotation without a name is a parenthesized type.
    std::string annotation(const Node &node, std::size_t depth) {
        const auto inner = text(node.children.front(), depth);
        return node.name.empty() ? '(' + inner + ')' : node.name + " :: " + inner;
    }

    // Containers and ranges; the other kinds go to `operations`.
    std::string compound(const Node &node, std::size_t depth) {
        switch (node.kind) {
        case Kind::range:
            return join(node.children, depth, "..");
        case Kind::tuple:
            return node.name == "any" ? "tuple()" : '{' + join(node.children, depth) + '}';
        case Kind::list:
            return list(node, depth);
        case Kind::map:
            return map(node, depth);
        case Kind::record:
            return record(node, depth);
        default:
            return operations(node, depth);
        }
    }

    // Bitstrings, funs, operators, annotations, unions and named types.
    std::string operations(const Node &node, std::size_t depth) {
        switch (node.kind) {
        case Kind::bitstring:
            return bitstring(node, depth);
        case Kind::function:
            return function(node, depth);
        case Kind::unary:
        case Kind::binary:
            return operation(node, depth);
        case Kind::annotation:
            return annotation(node, depth);
        case Kind::union_type:
            return join(node.children, depth, " | ");
        default:
            return named(node, depth);
        }
    }

    // The graph whose nodes are printed and the nodes left to print.
    const Graph &graph_;
    std::size_t budget_;
};
} // namespace

std::string type_source(const Graph &graph, Id type, std::size_t budget) {
    return Printer(graph, budget).text(type, 0);
}
} // namespace clause::semantic::types
