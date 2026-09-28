#include "inference.hpp"
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
    const auto &bindings = function.function->bindings;
    const auto binding = std::ranges::find(bindings, id, &Binding::expression);
    if (binding != bindings.end()) {
        return {graph.top(), binding->argument};
    }
    return {graph.top()};
}

struct Visit {
    // Explicit enter/exit frames avoid host recursion for deeply nested expressions.
    ast::ExprId expression;
    bool ready = false;
};

// Evaluate a postorder node after its children; unresolved calls remain unknown for now.
Fact evaluate(Inference &inference, const FunctionRef function, const ast::ExprId &id) {
    const auto &syntax = *function.module->syntax;
    if (const auto *group = std::get_if<ast::Group>(&syntax.expression(id).value)) {
        return inference.expressions.at(&syntax.expression(group->expression));
    }
    return leaf(inference, function, id);
}

// A shared work budget bounds the entire batch and erases relations as well as concrete types.
Fact body(Inference &inference, const FunctionRef function, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto &definition = std::get<ast::Function>(syntax.form(function.function->form).value);
    const auto root = definition.clauses.front().body.front();
    std::vector<Visit> pending{{root}};
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
    return inference.expressions.at(&syntax.expression(root));
}
} // namespace

std::unique_ptr<Inference> infer(const CallGraph &calls, const Limits limits) {
    auto result = std::make_unique<Inference>(limits);
    std::size_t work = 0;
    for (const auto function : calls.order) {
        const auto fact = body(*result, function, work);
        result->functions.emplace(function.function,
                                  Summary{std::vector<Id>(function.function->key.arity, result->graph.top()), fact});
    }
    return result;
}
} // namespace erlang_aot::semantic::types
