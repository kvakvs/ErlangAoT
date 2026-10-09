#include "inference_funs.hpp"
#include "../funs.hpp"
#include <charconv>

namespace clause::semantic::types {
namespace {
// A literal arity as a host size; none for an invalid one.
std::optional<std::size_t> arity_of(const Integer &arity) {
    std::size_t value = 0;
    const auto &digits = arity.decimal;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() && value <= 255
               ? std::optional{value}
               : std::nullopt;
}

// The result fact fun F/A sees: the function's summary as inferred so far, recorded so a later pass can check it.
Id local_result(Inference &inference, const Function *target) {
    const auto summary = target ? inference.functions.find(target) : inference.functions.end();
    const auto result = inference.opaque_funs || summary == inference.functions.end() ? inference.graph.top()
                                                                                      : summary->second.result.type;
    if (target && !inference.opaque_funs) {
        inference.fun_reads.insert_or_assign(target, result);
    }
    return result;
}
} // namespace

std::optional<Id> fun_reference_fact(Inference &inference, const FunctionRef function,
                                     const ast::Expression &expression) {
    Lattice lattice(inference.graph);
    if (const auto *reference = std::get_if<ast::LocalFunReference>(&expression.value)) {
        const auto arity = arity_of(reference->arity);
        if (!arity) {
            return lattice.category("fun");
        }
        // A fun naming an auto-imported builtin is erlang:F/A, whose result is unknown here.
        const bool builtin = function.function->builtin_funs.contains(&expression);
        const auto *target = builtin ? nullptr : fun_target(*function.module, *reference);
        return lattice.fun(*arity, local_result(inference, target));
    }
    if (const auto *reference = std::get_if<ast::RemoteFunReference>(&expression.value)) {
        const auto *literal = std::get_if<Integer>(&reference->arity);
        const auto arity = literal ? arity_of(*literal) : std::nullopt;
        return arity ? lattice.fun(*arity, inference.graph.top()) : lattice.category("fun");
    }
    return std::nullopt;
}

Id call_value(Lattice &lattice, const Id callee, const std::size_t arity) {
    auto &graph = lattice.graph();
    std::vector<Id> results;
    for (const auto member : lattice.members(callee)) {
        const auto &node = graph.get(member);
        if (member == graph.top() || (node.kind == Kind::function && node.name == "any_arguments")) {
            return graph.top();
        }
        if (node.kind == Kind::function && node.children.size() == arity + 1) {
            results.push_back(node.children.back());
        }
    }
    return lattice.join(results, 0);
}
} // namespace clause::semantic::types
