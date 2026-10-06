#include "inference.hpp"
#include "../bindings.hpp"
#include "../capabilities.hpp"
#include "../records.hpp"
#include "inference_bindings.hpp"
#include <algorithm>

namespace erlang_aot::semantic::types {
namespace {
// Infer only implementation syntax; specifications never narrow an input or result.
Fact leaf(Inference &inference, const FunctionRef function, const ast::ExprId &id, const BindingFacts &bindings) {
    auto &graph = inference.graph;
    if (const auto value = integer_literal(*function.module->syntax, id, 64)) {
        return {graph.intern({Kind::integer, std::to_string(*value)})};
    }
    return bindings.read(id);
}

struct Visit {
    // Explicit enter/exit frames avoid host recursion for deeply nested expressions.
    ast::ExprId expression;
    bool ready = false;
};

// Instantiate each projection with this call's actual fact, never a shared mutable type variable.
Fact call_result(const Inference &inference, const ast::Module &syntax, const ast::Expression &expression,
                 const ast::CallExpression &call) {
    const auto callee = inference.callees.at(&expression);
    const auto &summary = inference.functions.at(callee.function);
    if (!summary.result.argument) {
        return summary.result;
    }
    const auto argument = call.arguments.at(*summary.result.argument);
    return inference.expressions.at(&syntax.expression(argument));
}

// Bottom (a recursive call not yet summarized) adds nothing; otherwise only a relation common to both survives.
Fact merged(Graph &graph, const Fact previous, const Fact next) {
    if (previous == Fact{graph.bottom()}) {
        return next;
    }
    if (next == Fact{graph.bottom()}) {
        return previous;
    }
    return {graph.widen(previous.type, next.type),
            previous.argument == next.argument ? previous.argument : std::nullopt};
}

// Only relations common to every successful function candidate or case/if clause survive the join.
template <typename Clauses> Fact joined(Inference &inference, const ast::Module &syntax, const Clauses &clauses) {
    Fact result{inference.graph.bottom()};
    for (const auto &clause : clauses) {
        result = merged(inference.graph, result, inference.expressions.at(&syntax.expression(clause.body.back())));
    }
    return result;
}

// Evaluate a postorder node only after all source-order argument facts are available.
Fact evaluate(Inference &inference, const FunctionRef function, const ast::ExprId &id, BindingFacts &bindings,
              std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto &expression = syntax.expression(id);
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        if (function.function->services.contains(&expression) || record_info_call(syntax, expression.value)) {
            return {inference.graph.top()};
        }
        return call_result(inference, syntax, expression, *call);
    }
    if (const auto *group = std::get_if<ast::Group>(&syntax.expression(id).value)) {
        return inference.expressions.at(&syntax.expression(group->expression));
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        const auto fact = inference.expressions.at(&syntax.expression(match->right));
        bindings.publish(match->left, fact, work);
        return fact;
    }
    if (const auto *selection = std::get_if<ast::CaseExpression>(&expression.value)) {
        return joined(inference, syntax, selection->clauses);
    }
    if (const auto *choice = std::get_if<ast::IfExpression>(&expression.value)) {
        return joined(inference, syntax, choice->clauses);
    }
    if (const auto *block = std::get_if<ast::BlockExpression>(&expression.value)) {
        return inference.expressions.at(&syntax.expression(block->body.back()));
    }
    return leaf(inference, function, id, bindings);
}

// A shared work budget bounds the entire batch and erases relations as well as concrete types.
// Every recorded expression is listed so a later fixed-point round can discard it.
Fact body(Inference &inference, const FunctionRef function, std::size_t &work,
          std::vector<const ast::Expression *> &recorded) {
    const auto &syntax = *function.module->syntax;
    const auto &definition = std::get<ast::Function>(syntax.form(function.function->form).value);
    BindingFacts bindings(function, inference, work);
    const auto roots = function_roots(definition);
    std::vector<Visit> pending;
    for (auto root = roots.rbegin(); root != roots.rend(); ++root) {
        pending.push_back({*root});
    }
    while (!pending.empty()) {
        if (work >= inference.graph.limits().syntax_work) {
            return {inference.graph.exhausted()};
        }
        ++work;
        const auto visit = pending.back();
        pending.pop_back();
        const auto &expression = syntax.expression(visit.expression);
        if (visit.ready) {
            inference.expressions.emplace(&expression, evaluate(inference, function, visit.expression, bindings, work));
            recorded.push_back(&expression);
        } else {
            pending.push_back({visit.expression, true});
            const auto children = expression_children(*function.module, expression);
            for (auto child = children.rbegin(); child != children.rend(); ++child) {
                pending.push_back({*child});
            }
        }
    }
    return joined(inference, syntax, definition.clauses);
}

// Rounds after which a recursive component that has not converged widens every member to top.
constexpr std::size_t WIDENING_ROUNDS = 16;

// Replace a summary result, keeping every input unknown as for exported functions.
void summarize(Inference &inference, const FunctionRef function, const Fact result) {
    inference.functions.insert_or_assign(
        function.function, Summary{std::vector<Id>(function.function->key.arity, inference.graph.top()), result});
}

// Re-infer every member from the current assumptions, discarding the previous round's expression facts.
bool refine(Inference &inference, const Component &component, std::size_t &work,
            std::vector<const ast::Expression *> &recorded) {
    for (const auto *expression : recorded) {
        inference.expressions.erase(expression);
    }
    recorded.clear();
    bool changed = false;
    for (const auto member : component.members) {
        auto &summary = inference.functions.at(member.function);
        const auto next = merged(inference.graph, summary.result, body(inference, member, work, recorded));
        changed = changed || next != summary.result;
        summary.result = next;
    }
    return changed;
}

// Iterate from bottom to a fixed point; non-convergence visibly widens all members to top and records final facts.
void solve(Inference &inference, const Component &component, std::size_t &work) {
    for (const auto member : component.members) {
        summarize(inference, member, {inference.graph.bottom()});
    }
    std::vector<const ast::Expression *> recorded;
    for (std::size_t round = 0; round < WIDENING_ROUNDS; ++round) {
        if (!refine(inference, component, work, recorded)) {
            return;
        }
    }
    for (const auto member : component.members) {
        summarize(inference, member, {inference.graph.exhausted()});
    }
    (void)refine(inference, component, work, recorded);
}
} // namespace

std::unique_ptr<Inference> infer(const CallGraph &calls, const Limits limits) {
    auto result = std::make_unique<Inference>(limits);
    for (const auto &call : calls.calls) {
        result->callees.emplace(&call.caller.module->syntax->expression(call.expression), call.callee);
    }
    std::size_t work = 0;
    std::vector<const ast::Expression *> recorded;
    for (const auto &component : calls.components) {
        if (component.recursive) {
            solve(*result, component, work);
        } else {
            const auto function = component.members.front();
            recorded.clear();
            summarize(*result, function, body(*result, function, work, recorded));
        }
    }
    return result;
}
} // namespace erlang_aot::semantic::types
