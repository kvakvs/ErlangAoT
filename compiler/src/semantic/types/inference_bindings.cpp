#include "inference_bindings.hpp"

namespace clause::semantic::types {
namespace {
// Every event or alias step consumes the shared inference budget before adding a fact.
bool spend(const Inference &inference, std::size_t &work) {
    if (work >= inference.graph.limits().syntax_work) {
        return false;
    }
    ++work;
    return true;
}

// Containers require extraction proofs, so only groups and whole-value aliases preserve this fact.
std::vector<ast::ExprId> aliases(const ast::ExprValue &value) {
    if (const auto *group = std::get_if<ast::Group>(&value)) {
        return {group->expression};
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&value)) {
        return {match->left, match->right};
    }
    return {};
}
} // namespace

BindingFacts::BindingFacts(const FunctionRef owner, Inference &facts, std::size_t &work)
    : function(owner), inference(facts) {
    for (const auto &binding : function.function->bindings) {
        if (!spend(inference, work)) {
            return;
        }
        const auto &expression = function.module->syntax->expression(binding.expression);
        events.emplace(&expression, &binding);
        const auto &definition =
            function.function->clause_bindings.at(binding.identity.clause).definitions.at(binding.identity.local);
        if (!values.try_emplace(binding.identity, Fact{inference.graph.top(), definition.argument}).second &&
            binding.use == BindingUse::definition) {
            shared.insert(binding.identity);
        }
    }
}

Fact BindingFacts::read(const ast::ExprId &id) const {
    const auto event = events.find(&function.module->syntax->expression(id));
    if (event != events.end() && event->second->use == BindingUse::read) {
        const auto fact = values.find(event->second->identity);
        if (fact != values.end()) {
            return fact->second;
        }
    }
    return {inference.graph.top()};
}

void BindingFacts::publish(const ast::ExprId &pattern, Fact fact, std::size_t &work) {
    std::vector<ast::ExprId> pending{pattern};
    while (!pending.empty()) {
        if (!spend(inference, work)) {
            return;
        }
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = function.module->syntax->expression(id);
        const auto event = events.find(&expression);
        if (event != events.end() && event->second->use == BindingUse::definition &&
            !shared.contains(event->second->identity)) {
            values.insert_or_assign(event->second->identity, fact);
        }
        const auto children = aliases(expression.value);
        pending.insert(pending.end(), children.begin(), children.end());
    }
}
} // namespace clause::semantic::types
