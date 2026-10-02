#include "inference.hpp"
#include "../bindings.hpp"
#include "../capabilities.hpp"
#include <algorithm>

namespace erlang_aot::semantic::types {
namespace {
// Infer only implementation syntax; specifications never narrow an input or result.
Fact leaf(Inference &inference, const FunctionRef function, const ast::ExprId &id) {
    auto &graph = inference.graph;
    if (const auto value = integer_literal(*function.module->syntax, id, 64)) {
        return {graph.intern({Kind::integer, std::to_string(*value)})};
    }
    return {graph.top(), binding_argument(*function.function, id)};
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

// Evaluate a postorder node only after all source-order argument facts are available.
Fact evaluate(Inference &inference, const FunctionRef function, const ast::ExprId &id) {
    const auto &syntax = *function.module->syntax;
    const auto &expression = syntax.expression(id);
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        if (function.function->services.contains(&expression)) {
            return {inference.graph.top()};
        }
        return call_result(inference, syntax, expression, *call);
    }
    if (const auto *group = std::get_if<ast::Group>(&syntax.expression(id).value)) {
        return inference.expressions.at(&syntax.expression(group->expression));
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        return inference.expressions.at(&syntax.expression(match->right));
    }
    return leaf(inference, function, id);
}

// Only relations common to every successful candidate survive the function summary.
Fact joined_result(Inference &inference, const ast::Module &syntax, const ast::Function &definition) {
    auto result = inference.expressions.at(&syntax.expression(definition.clauses.front().body.back()));
    for (const auto &clause : definition.clauses) {
        const auto fact = inference.expressions.at(&syntax.expression(clause.body.back()));
        result.type = inference.graph.widen(result.type, fact.type);
        if (result.argument != fact.argument) {
            result.argument.reset();
        }
    }
    return result;
}

// A shared work budget bounds the entire batch and erases relations as well as concrete types.
Fact body(Inference &inference, const FunctionRef function, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto &definition = std::get<ast::Function>(syntax.form(function.function->form).value);
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
            inference.expressions.emplace(&expression, evaluate(inference, function, visit.expression));
        } else {
            pending.push_back({visit.expression, true});
            const auto children = expression_children(expression);
            for (auto child = children.rbegin(); child != children.rend(); ++child) {
                pending.push_back({*child});
            }
        }
    }
    return joined_result(inference, syntax, definition);
}
} // namespace

std::unique_ptr<Inference> infer(const CallGraph &calls, const Limits limits) {
    auto result = std::make_unique<Inference>(limits);
    for (const auto &call : calls.calls) {
        result->callees.emplace(&call.caller.module->syntax->expression(call.expression), call.callee);
    }
    std::size_t work = 0;
    for (const auto function : calls.order) {
        const auto fact = body(*result, function, work);
        result->functions.emplace(function.function,
                                  Summary{std::vector<Id>(function.function->key.arity, result->graph.top()), fact});
    }
    return result;
}
} // namespace erlang_aot::semantic::types
