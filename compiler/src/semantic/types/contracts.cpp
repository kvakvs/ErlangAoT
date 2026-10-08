#include "contracts.hpp"
#include "../features.hpp"
#include <algorithm>

namespace clause::semantic::types {
namespace {
// Compare only precise implementation singletons; top and parameter relations are inconclusive here.
bool excludes(Registry &declared, const Inference &inferred, const Fact fact, const Id expected,
              const std::string_view owner) {
    const auto &node = inferred.graph.get(fact.type);
    return node.kind == Kind::integer && excludes_integer(declared, node.name, expected, owner);
}

// Function identity is batch-owned, so overloaded contracts cannot leak across targets.
const Contract *contract_for(const Registry &declared, const FunctionRef function) {
    const Key key{utf8(function.module->name), utf8(function.function->key.name), function.function->key.arity};
    const auto found = std::ranges::find_if(
        declared.contracts, [&](const auto &contract) { return !contract.callback && contract.key == key; });
    return found == declared.contracts.end() ? nullptr : &*found;
}

// Constraints and widened signatures remain uncertain rather than manufacturing a proof.
std::optional<Node> signature(const Registry &declared, const Overload &overload) {
    auto node = declared.graph.get(overload.function);
    if (!overload.constraints.empty() || node.kind != Kind::function || node.children.empty()) {
        return {};
    }
    const auto inputs = std::span(node.children).first(node.children.size() - 1);
    if (std::ranges::contains(inputs, declared.graph.bottom())) {
        return {};
    }
    return node;
}

// A result warning requires every overload to reject the same known implementation value.
void result_contract(Registry &declared, const Inference &inferred, const FunctionRef function, const Reporter &out) {
    const auto *contract = contract_for(declared, function);
    if (!contract) {
        return;
    }
    const auto fact = inferred.functions.at(function.function).result;
    const bool rejected = std::ranges::all_of(contract->overloads, [&](const auto &overload) {
        const auto node = signature(declared, overload);
        return node && excludes(declared, inferred, fact, node->children.back(), contract->key.module);
    });
    if (rejected) {
        report(*function.module, &function.module->syntax->form(contract->form).source,
               "inferred result contradicts specification for " + contract->key.name, out, Severity::warning);
    }
}

// At least one known argument must rule out an overload; missing facts remain unknown.
bool rejects_arguments(Registry &declared, const Inference &inferred, const Call &call, const Overload &overload) {
    const auto node = signature(declared, overload);
    if (!node) {
        return false;
    }
    const auto &syntax = *call.caller.module->syntax;
    const auto &arguments = std::get<ast::CallExpression>(syntax.expression(call.expression).value).arguments;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto fact = inferred.expressions.find(&syntax.expression(arguments[i]));
        if (fact != inferred.expressions.end() &&
            excludes(declared, inferred, fact->second, node->children.at(i), utf8(call.caller.module->name))) {
            return true;
        }
    }
    return false;
}

// Calls remain executable even when no declared alternative admits their inferred arguments.
void call_contract(Registry &declared, const Inference &inferred, const Call &call, const Reporter &out) {
    const auto *contract = contract_for(declared, call.callee);
    if (!contract) {
        return;
    }
    if (std::ranges::all_of(contract->overloads, [&](const auto &overload) {
            return rejects_arguments(declared, inferred, call, overload);
        })) {
        report(*call.caller.module, &call.caller.module->syntax->expression(call.expression).source,
               "inferred arguments contradict specification for " + contract->key.name, out, Severity::warning);
    }
}
} // namespace

void check_contracts(Registry &declared, const Inference &inferred, const CallGraph &calls, const Reporter &out) {
    for (const auto function : calls.order) {
        result_contract(declared, inferred, function, out);
    }
    for (const auto &call : calls.calls) {
        call_contract(declared, inferred, call, out);
    }
}
} // namespace clause::semantic::types
