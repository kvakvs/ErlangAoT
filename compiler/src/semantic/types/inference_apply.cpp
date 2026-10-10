#include "inference_apply.hpp"
#include "../capabilities.hpp"
#include "inference_funs.hpp"

namespace clause::semantic::types {
namespace {
// The facts of a literal proper list's elements; none for another expression.
std::optional<std::vector<Fact>> literal_elements(const Inference &inference, const ast::Module &syntax,
                                                  const ast::ExprId &list) {
    const auto *value = std::get_if<ast::List>(&syntax.expression(ungroup(syntax, list)).value);
    if (!value || value->tail) {
        return std::nullopt;
    }
    std::vector<Fact> result;
    result.reserve(value->elements.size());
    for (const auto &element : value->elements) {
        const auto found = inference.expressions.find(&syntax.expression(element));
        result.push_back(found == inference.expressions.end() ? Fact{inference.graph.top()} : found->second);
    }
    return result;
}

// The elements of a positional list fact ending in `[]`; none for one ending in another tail.
std::optional<std::vector<Fact>> positional_elements(Lattice &lattice, const Node &node) {
    const auto &tail = lattice.graph().get(node.children.back());
    if (tail.kind != Kind::list || !tail.children.empty()) {
        return std::nullopt;
    }
    std::vector<Fact> result;
    result.reserve(node.children.size() - 1);
    for (std::size_t index = 0; index + 1 < node.children.size(); ++index) {
        result.push_back({node.children[index]});
    }
    return result;
}

// The argument facts a list fact holds: none for `[]`, its elements for a proper list of known positions, `arity`
// copies of a proper list's element; none when its length is unknown.
std::optional<std::vector<Fact>> fact_elements(Lattice &lattice, const Id list,
                                               const std::optional<std::size_t> arity) {
    const auto node = lattice.graph().get(list);
    if (node.kind == Kind::positional) {
        return positional_elements(lattice, node);
    }
    if (node.kind != Kind::list) {
        return std::nullopt;
    }
    if (node.children.empty()) {
        return std::vector<Fact>{};
    }
    // A list of unknown length: as many arguments as the fun takes; a call of another length raises.
    if (!arity || (*arity == 0 && node.name == "nonempty")) {
        return std::nullopt;
    }
    return std::vector<Fact>(*arity, Fact{node.children.front()});
}
} // namespace

std::optional<std::vector<Fact>> applied_arguments(const Inference &inference, Lattice &lattice,
                                                   const ast::Module &syntax, const ast::ExprId &list,
                                                   const std::optional<std::size_t> arity) {
    if (auto elements = literal_elements(inference, syntax, list)) {
        return elements;
    }
    const auto found = inference.expressions.find(&syntax.expression(list));
    return found == inference.expressions.end() ? std::nullopt : fact_elements(lattice, found->second.type, arity);
}

std::optional<std::size_t> fun_arity(Lattice &lattice, const Id fact) {
    std::optional<std::size_t> result;
    for (const auto member : lattice.members(fact)) {
        const auto &node = lattice.graph().get(member);
        if (node.kind != Kind::function || node.children.empty() || node.name == "any_arguments") {
            return std::nullopt;
        }
        // An overloaded fun's types share one arity: the first one tells it.
        const auto &type = node.name == "clauses" ? lattice.graph().get(node.children.front()) : node;
        const auto arity = type.children.size() - 1;
        if (result && *result != arity) {
            return std::nullopt;
        }
        result = arity;
    }
    return result;
}

std::optional<Id> named_fun(Inference &inference, const Named &target, const std::size_t arity) {
    const auto &module_node = inference.graph.get(target.module);
    const auto &name_node = inference.graph.get(target.name);
    if (module_node.kind != Kind::atom || name_node.kind != Kind::atom) {
        return std::nullopt;
    }
    const auto found = inference.exported.find({module_node.name, name_node.name, arity});
    if (found == inference.exported.end()) {
        return std::nullopt;
    }
    return function_fun(inference, found->second, arity);
}
} // namespace clause::semantic::types
