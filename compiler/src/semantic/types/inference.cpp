#include "inference.hpp"
#include "../bindings.hpp"
#include "../capabilities.hpp"
#include "../funs.hpp"
#include "../records.hpp"
#include "function_types.hpp"
#include "inference_bindings.hpp"
#include "inference_funs.hpp"
#include "inference_inputs.hpp"
#include "inference_operators.hpp"
#include "inference_scopes.hpp"
#include "inference_uses.hpp"
#include "inference_values.hpp"
#include "lattice.hpp"
#include <algorithm>
#include <optional>

namespace clause::semantic::types {
namespace {
// Infer only implementation syntax; specifications never narrow an input or result.
Fact leaf(Inference &inference, const FunctionRef function, const ast::ExprId &id, const BindingFacts &bindings) {
    const auto &syntax = *function.module->syntax;
    if (const auto fact = constructed_fact(inference, function, syntax.expression(id).value)) {
        return {*fact};
    }
    return bindings.read(id);
}

// Clause bodies that can never run.
using Impossible = std::set<const std::vector<ast::ExprId> *>;

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

// The value of a body: its last expression's, or none() when an expression before it never completes.
Fact sequence(const Inference &inference, const ast::Module &syntax, const std::vector<ast::ExprId> &body) {
    for (const auto &expression : body) {
        const auto fact = inference.expressions.at(&syntax.expression(expression));
        if (fact.type == inference.graph.bottom()) {
            return fact;
        }
    }
    return inference.expressions.at(&syntax.expression(body.back()));
}

// Only relations common to every successful function candidate or case/if clause survive the join; a clause that
// can never run adds nothing.
template <typename Clauses>
Fact joined(Inference &inference, const ast::Module &syntax, const Clauses &clauses,
            const Impossible *impossible = nullptr) {
    Fact result{inference.graph.bottom()};
    for (const auto &clause : clauses) {
        if (!impossible || !impossible->contains(&clause.body)) {
            result = merged(inference.graph, result, sequence(inference, syntax, clause.body));
        }
    }
    return result;
}

// The recorded fact of an expression; term() for one that is not evaluated.
Fact recorded_fact(const Inference &inference, const ast::Module &syntax, const ast::ExprId &id) {
    const auto found = inference.expressions.find(&syntax.expression(id));
    return found == inference.expressions.end() ? Fact{inference.graph.top()} : found->second;
}

// Whether a call never happens: its called value or one of its arguments never produces a value.
bool unreachable_call(const Inference &inference, const ast::Module &syntax, const ast::CallExpression &call) {
    // record_info/2 and named functions have no evaluated operands: they read as term().
    const auto never = [&](const ast::ExprId &operand) {
        return recorded_fact(inference, syntax, operand).type == inference.graph.bottom();
    };
    return never(call.target) || std::ranges::any_of(call.arguments, never);
}

// Whether a call's result is unknown: a service, record_info/2 or a dynamic call.
bool opaque_call(const FunctionRef function, const ast::Expression &expression, const ast::CallExpression &call) {
    const auto &syntax = *function.module->syntax;
    return function.function->services.contains(&expression) || record_info_call(syntax, expression.value) ||
           dynamic_call(syntax, call);
}

// The guard tests and bodies of fun clauses, clause by clause.
std::vector<ast::ExprId> clause_roots(const std::vector<ast::FunctionClause> &clauses) {
    std::vector<ast::ExprId> result;
    for (const auto &clause : clauses) {
        for (const auto &alternative :
             clause.guard ? clause.guard->alternatives : std::vector<ast::GuardConjunction>{}) {
            result.insert(result.end(), alternative.tests.begin(), alternative.tests.end());
        }
        result.insert(result.end(), clause.body.begin(), clause.body.end());
    }
    return result;
}

// A receive's value: its clauses' joined values, and its after body's unless the timeout is infinity.
Fact receive_fact(Inference &inference, const ast::Module &syntax, const ast::ReceiveExpression &receive,
                  const Impossible &impossible) {
    const auto clauses = joined(inference, syntax, receive.clauses, &impossible);
    if (!receive.after) {
        return clauses;
    }
    const auto &timeout = inference.graph.get(recorded_fact(inference, syntax, receive.after->timeout).type);
    if (timeout.kind == Kind::atom && timeout.name == "infinity") {
        return clauses;
    }
    return merged(inference.graph, clauses, sequence(inference, syntax, receive.after->body));
}

// A try's value: its of clauses' joined values (its body's without of) joined with its catch clauses' values.
Fact try_fact(Inference &inference, const ast::Module &syntax, const ast::TryExpression &attempt,
              const Impossible &impossible) {
    const auto result =
        attempt.of ? joined(inference, syntax, *attempt.of, &impossible) : sequence(inference, syntax, attempt.body);
    if (!attempt.handlers) {
        return result;
    }
    return merged(inference.graph, result, joined(inference, syntax, *attempt.handlers, &impossible));
}

// A maybe's value: its body's when every ?= match can succeed, joined with its else clauses' values, or without
// else, with the values its ?= matches fail on.
Fact maybe_fact(Inference &inference, const ast::Module &syntax, const ast::Expression &expression,
                const BindingFacts &bindings) {
    const auto &conditional = std::get<ast::MaybeExpression>(expression.value);
    const auto body = bindings.stopped(expression) ? Fact{inference.graph.bottom()}
                                                   : sequence(inference, syntax, maybe_operands(conditional));
    const auto rest = conditional.otherwise ? joined(inference, syntax, *conditional.otherwise, &bindings.impossible)
                                            : Fact{bindings.failures(expression)};
    return merged(inference.graph, body, rest);
}

// The joined clause values of a case, if, receive, try or maybe; none for other expressions.
std::optional<Fact> selection_fact(Inference &inference, const ast::Module &syntax, const ast::Expression &expression,
                                   const BindingFacts &bindings) {
    const auto &value = expression.value;
    const auto &impossible = bindings.impossible;
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        return joined(inference, syntax, selection->clauses, &impossible);
    }
    if (const auto *choice = std::get_if<ast::IfExpression>(&value)) {
        return joined(inference, syntax, choice->clauses, &impossible);
    }
    if (const auto *receive = std::get_if<ast::ReceiveExpression>(&value)) {
        return receive_fact(inference, syntax, *receive, impossible);
    }
    if (const auto *attempt = std::get_if<ast::TryExpression>(&value)) {
        return try_fact(inference, syntax, *attempt, impossible);
    }
    if (std::holds_alternative<ast::MaybeExpression>(value)) {
        return maybe_fact(inference, syntax, expression, bindings);
    }
    return std::nullopt;
}

// Anonymous funs called where they are bound are evaluated again for each call, at most this many deep.
constexpr std::size_t INSTANCE_DEPTH = 4;

bool walk(Inference &inference, FunctionRef function, BindingFacts &bindings, std::vector<Frame> pending,
          std::size_t &work, std::vector<const ast::Expression *> &recorded);

// Every expression evaluated under `roots`.
std::vector<const ast::Expression *> subtree(const Module &module, const std::vector<ast::ExprId> &roots) {
    std::vector<const ast::Expression *> result;
    std::vector<ast::ExprId> pending(roots.begin(), roots.end());
    while (!pending.empty()) {
        const auto &expression = module.syntax->expression(pending.back());
        pending.pop_back();
        result.push_back(&expression);
        const auto children = expression_children(module, expression);
        pending.insert(pending.end(), children.begin(), children.end());
    }
    return result;
}

// The facts of a function's walk that evaluating an anonymous fun again for a call changes, restored afterwards.
struct Instance {
    std::map<BindingId, Fact> values;
    std::map<const ast::Expression *, std::vector<Waiting>> waiting;
    std::map<std::pair<const ast::Expression *, std::size_t>, BindingFacts::Conditional> conditionals;
    Impossible impossible;
    std::map<std::pair<const ast::Expression *, std::size_t>, FunctionType> fun_inputs;
    std::map<const ast::Expression *, std::vector<Fact>> instances;
};

// Keep the facts an evaluation for a call changes.
Instance keep(const BindingFacts &bindings) {
    return {bindings.values,     bindings.waiting,    bindings.conditionals,
            bindings.impossible, bindings.fun_inputs, bindings.instances};
}

// Restore the facts kept before an evaluation for a call.
void restore(BindingFacts &bindings, const Instance &kept) {
    bindings.values = kept.values;
    bindings.waiting = kept.waiting;
    bindings.conditionals = kept.conditionals;
    bindings.impossible = kept.impossible;
    bindings.fun_inputs = kept.fun_inputs;
    bindings.instances = kept.instances;
}

// The result of calling anonymous fun `lambda` with arguments of facts `arguments`: its clauses are entered, guarded
// and left again like a case's, their patterns matching the arguments (a clause they cannot match adds nothing),
// then the facts of its first evaluation are restored.
Fact instantiate(Inference &inference, const FunctionRef function, BindingFacts &bindings, const ast::ExprId &lambda,
                 const std::vector<Fact> &arguments, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto &expression = syntax.expression(lambda);
    const auto &clauses = *fun_clauses(expression.value);
    if (clauses.front().arguments.size() != arguments.size()) {
        return {inference.graph.bottom()};
    }
    std::vector<std::pair<const ast::Expression *, Fact>> saved;
    for (const auto *evaluated : subtree(*function.module, clause_roots(clauses))) {
        if (const auto found = inference.expressions.find(evaluated); found != inference.expressions.end()) {
            saved.emplace_back(*found);
            inference.expressions.erase(found);
        }
    }
    const auto kept = keep(bindings);
    bindings.instances.insert_or_assign(&expression, arguments);
    ++bindings.depth;
    std::vector<const ast::Expression *> scratch;
    std::vector<Frame> frames;
    expand_clauses(*function.module, lambda, frames);
    const auto result = walk(inference, function, bindings, std::move(frames), work, scratch)
                            ? joined(inference, syntax, clauses, &bindings.impossible)
                            : Fact{inference.graph.exhausted()};
    for (const auto *evaluated : scratch) {
        inference.expressions.erase(evaluated);
    }
    inference.expressions.insert(saved.begin(), saved.end());
    restore(bindings, kept);
    --bindings.depth;
    return result;
}

// The facts of a call's arguments.
std::vector<Fact> argument_facts(const Inference &inference, const ast::Module &syntax,
                                 const ast::CallExpression &call) {
    std::vector<Fact> result;
    result.reserve(call.arguments.size());
    for (const auto &argument : call.arguments) {
        result.push_back(recorded_fact(inference, syntax, argument));
    }
    return result;
}

// A call of a value: an anonymous fun bound to the called variable is evaluated with the arguments; otherwise the
// value's fact gives the result.
Fact value_call(Inference &inference, const FunctionRef function, const ast::CallExpression &call,
                BindingFacts &bindings, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto target = ungroup(syntax, call.target);
    const auto arguments = argument_facts(inference, syntax, call);
    if (const auto lambda = bindings.lambda(target); lambda && bindings.depth < INSTANCE_DEPTH) {
        return instantiate(inference, function, bindings, *lambda, arguments, work);
    }
    Lattice lattice(inference.graph);
    return {call_value(lattice, recorded_fact(inference, syntax, target).type, arguments)};
}

// A fun value: fun F/A and fun M:F/A by the function they name, an anonymous fun by the function types of its
// possible clauses: their arguments' facts after head and guard, and their results.
std::optional<Fact> fun_value_fact(Inference &inference, const FunctionRef function, const ast::Expression &expression,
                                   const BindingFacts &bindings) {
    if (const auto fact = fun_reference_fact(inference, function, expression)) {
        return Fact{*fact};
    }
    const auto *clauses = fun_clauses(expression.value);
    if (!clauses) {
        return std::nullopt;
    }
    const auto arity = clauses->front().arguments.size();
    std::vector<FunctionType> types;
    for (std::size_t index = 0; index < clauses->size(); ++index) {
        const auto &clause = clauses->at(index);
        if (bindings.impossible.contains(&clause.body)) {
            continue;
        }
        const auto found = bindings.fun_inputs.find({&expression, index});
        auto type = found == bindings.fun_inputs.end()
                        ? FunctionType{std::vector<Id>(arity, inference.graph.top()), {inference.graph.bottom()}}
                        : found->second;
        // A relation of the result is to the enclosing function's arguments, not the fun's.
        type.result = {sequence(inference, *function.module->syntax, clause.body).type};
        types.push_back(std::move(type));
    }
    Lattice lattice(inference.graph);
    if (types.empty()) {
        return Fact{lattice.fun(arity, inference.graph.bottom())};
    }
    return Fact{fun_fact(lattice, merge_types(inference.graph, std::move(types)))};
}

// Narrow a returned call's variable arguments to `facts`, position by position.
void narrow_arguments(BindingFacts &bindings, const ast::CallExpression &call, const std::vector<Id> &facts) {
    for (std::size_t index = 0; index < call.arguments.size() && index < facts.size(); ++index) {
        bindings.narrow(call.arguments[index], facts[index]);
    }
}

// A call: none() when it never happens, else by its callee: a value, a function or an unknown target.
Fact call_fact(Inference &inference, const FunctionRef function, const ast::Expression &expression,
               const ast::CallExpression &call, BindingFacts &bindings, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    if (unreachable_call(inference, syntax, call)) {
        return {inference.graph.bottom()};
    }
    if (fun_call(syntax, call)) {
        return value_call(inference, function, call, bindings, work);
    }
    if (opaque_call(function, expression, call)) {
        return {inference.graph.top()};
    }
    const auto *callee = inference.callees.at(&expression).function;
    const auto &summary = inference.functions.at(callee);
    // A function with function types: the call reads those its arguments select (docs/semantic.md#inference).
    auto selection = summary.types.empty()
                         ? Selection{call_result(inference, syntax, expression, call)}
                         : select(inference.graph, summary.types, argument_facts(inference, syntax, call));
    // A call that returned had arguments within the callee's success domain and the selected types' inputs, once
    // they are final.
    if (selection.result.type != inference.graph.bottom() && !inference.solving.contains(callee)) {
        narrow_arguments(bindings, call, summary.inputs);
        narrow_arguments(bindings, call, selection.inputs);
    }
    return selection.result;
}

// Groups, matches and blocks have the value of the expression they end with; a match publishes its pattern.
std::optional<Fact> passed_fact(Inference &inference, const FunctionRef function, const ast::Expression &expression,
                                BindingFacts &bindings, std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    if (const auto *group = std::get_if<ast::Group>(&expression.value)) {
        return inference.expressions.at(&syntax.expression(group->expression));
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        const auto fact = inference.expressions.at(&syntax.expression(match->right));
        bindings.publish(match->left, fact, work);
        bindings.bind_lambda(*match);
        return fact;
    }
    if (const auto *block = std::get_if<ast::BlockExpression>(&expression.value)) {
        return sequence(inference, syntax, block->body);
    }
    return std::nullopt;
}

// Evaluate a postorder node only after all source-order argument facts are available.
Fact evaluate(Inference &inference, const FunctionRef function, const ast::ExprId &id, BindingFacts &bindings,
              std::size_t &work) {
    const auto &syntax = *function.module->syntax;
    const auto &expression = syntax.expression(id);
    if (const auto fact = operation_fact(inference, function, expression)) {
        return {*fact};
    }
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        return call_fact(inference, function, expression, *call, bindings, work);
    }
    if (const auto fact = passed_fact(inference, function, expression, bindings, work)) {
        return *fact;
    }
    if (const auto fact = fun_value_fact(inference, function, expression, bindings)) {
        return *fact;
    }
    if (const auto fact = selection_fact(inference, syntax, expression, bindings)) {
        return *fact;
    }
    return leaf(inference, function, id, bindings);
}

// Run the walk's frames: evaluate expressions in postorder, recording each one's fact and listing it in `recorded`,
// and run the scope steps between them; false when the shared work budget ran out.
bool walk(Inference &inference, const FunctionRef function, BindingFacts &bindings, std::vector<Frame> pending,
          std::size_t &work, std::vector<const ast::Expression *> &recorded) {
    const auto &syntax = *function.module->syntax;
    while (!pending.empty()) {
        if (work >= inference.graph.limits().syntax_work) {
            return false;
        }
        ++work;
        const auto frame = pending.back();
        pending.pop_back();
        const auto &expression = syntax.expression(frame.expression);
        if (frame.step == Step::ready) {
            const auto [fact, added] = inference.expressions.emplace(
                &expression, evaluate(inference, function, frame.expression, bindings, work));
            (void)added;
            recorded.push_back(&expression);
            bindings.matched(expression, fact->second, work);
            if (fact->second.type != inference.graph.bottom()) {
                narrow_uses(bindings, expression);
            }
        } else if (frame.step == Step::visit) {
            bindings.expect(expression.value);
            expand(*function.module, frame.expression, pending);
        } else {
            scope_step(bindings, frame, work);
        }
    }
    return true;
}

// A function body's result and the entry domain of its arguments.
struct Body {
    Fact result;
    // The inputs a summary shows (the success domain, or the entry domain of a function that never returns) and the
    // entry domain.
    std::vector<Id> domain;
    std::vector<Id> entry;
    // The function types of its possible clauses, merged within their budget.
    std::vector<FunctionType> types = {};
};

// A shared work budget bounds the entire batch and erases relations as well as concrete types.
// Every recorded expression is listed so a later fixed-point round can discard it.
Body body(Inference &inference, const FunctionRef function, std::size_t &work,
          std::vector<const ast::Expression *> &recorded) {
    const auto &syntax = *function.module->syntax;
    const auto &definition = std::get<ast::Function>(syntax.form(function.function->form).value);
    BindingFacts bindings(function, inference, work);
    // Each clause's head matches the inputs; the arguments' facts at clause entry join into the entry domain.
    bindings.inputs = inputs_of(inference, *function.function);
    bindings.domain.assign(bindings.inputs.size(), inference.graph.bottom());
    bindings.success = bindings.domain;
    std::vector<Frame> frames;
    expand_heads(definition, frames);
    if (!walk(inference, function, bindings, std::move(frames), work, recorded)) {
        return {{inference.graph.exhausted()}, bindings.inputs, bindings.inputs};
    }
    // A function that returns shows its success domain; one that never does, its entry domain.
    const auto result = joined(inference, syntax, definition.clauses, &bindings.impossible);
    return {result, result.type == inference.graph.bottom() ? bindings.domain : bindings.success, bindings.domain,
            merge_types(inference.graph, std::move(bindings.types))};
}

// Rounds of plain joins before results widen (docs/semantic.md#inference-domain), as many as the singleton budget:
// cycles of up to that many functions converge exactly.
constexpr std::size_t JOIN_ROUNDS = 8;
// Widening passes over a component after the join rounds: a widened fact moves around a cycle one member per round,
// and an integer fact widens at most three times (to a threshold, the next, unbounded). A component that has not
// converged by then widens every member to top.
constexpr std::size_t WIDENING_PASSES = 4;

// Replace a summary: its result and the entry domain of its arguments (by default the inputs the body starts from).
void summarize(Inference &inference, const FunctionRef function, const Fact result,
               std::optional<std::vector<Id>> domain = std::nullopt) {
    inference.functions.insert_or_assign(
        function.function, Summary{domain ? std::move(*domain) : inputs_of(inference, *function.function), result});
}

// Re-infer every member from the current assumptions, discarding the previous round's expression facts; results
// join with the previous round's, or widen once `widening`.
bool refine(Inference &inference, const Component &component, std::size_t &work,
            std::vector<const ast::Expression *> &recorded, bool widening) {
    for (const auto *expression : recorded) {
        inference.expressions.erase(expression);
    }
    recorded.clear();
    bool changed = false;
    for (const auto member : component.members) {
        auto evaluated = body(inference, member, work, recorded);
        auto &summary = inference.functions.at(member.function);
        const auto next = merged(inference.graph, summary.result, evaluated.result, widening);
        // Calls read the function types too: they converge with the results.
        auto types = next_round(inference.graph, summary.types, std::move(evaluated.types), widening);
        changed = changed || next != summary.result || types != summary.types;
        summary.result = next;
        summary.inputs = std::move(evaluated.domain);
        summary.entry = std::move(evaluated.entry);
        summary.types = std::move(types);
    }
    return changed;
}

// The members of a solved component have final summaries.
void finish(Inference &inference, const Component &component) {
    for (const auto member : component.members) {
        inference.solving.erase(member.function);
    }
}

// Iterate from bottom to a fixed point; non-convergence visibly widens all members to top and records final facts.
void solve(Inference &inference, const Component &component, std::size_t &work) {
    for (const auto member : component.members) {
        summarize(inference, member, {inference.graph.bottom()});
        inference.solving.insert(member.function);
    }
    std::vector<const ast::Expression *> recorded;
    const auto rounds = JOIN_ROUNDS + WIDENING_PASSES * component.members.size();
    for (std::size_t round = 0; round < rounds; ++round) {
        if (!refine(inference, component, work, recorded, round >= JOIN_ROUNDS)) {
            finish(inference, component);
            return;
        }
    }
    for (const auto member : component.members) {
        summarize(inference, member, {inference.graph.exhausted()});
    }
    (void)refine(inference, component, work, recorded, false);
    // A component that did not converge keeps only its widened union summaries.
    for (const auto member : component.members) {
        inference.functions.at(member.function).types.clear();
    }
    finish(inference, component);
}

// Passes over the batch that join local inputs, then passes that widen them, before inputs and fun references are
// given up as unknown.
constexpr std::size_t PASSES = JOIN_ROUNDS + 2 * WIDENING_PASSES;

// Infer every component in callee-before-caller order, from fresh expression facts and its own work budget.
void infer_pass(Inference &inference, const CallGraph &calls) {
    inference.expressions.clear();
    inference.fun_reads.clear();
    std::size_t work = 0;
    std::vector<const ast::Expression *> recorded;
    for (const auto &component : calls.components) {
        if (component.recursive) {
            solve(inference, component, work);
        } else {
            const auto function = component.members.front();
            recorded.clear();
            auto evaluated = body(inference, function, work, recorded);
            summarize(inference, function, evaluated.result, std::move(evaluated.domain));
            auto &summary = inference.functions.at(function.function);
            summary.entry = std::move(evaluated.entry);
            summary.types = std::move(evaluated.types);
        }
    }
}

// Whether every fun F/A of the pass saw the final facts of its function.
bool settled(Inference &inference) {
    Lattice lattice(inference.graph);
    return std::ranges::all_of(inference.fun_reads, [&](const auto &read) {
        return summary_fun(lattice, inference.functions.at(read.first), read.first->key.arity) == read.second;
    });
}
} // namespace

std::unique_ptr<Inference> infer(const CallGraph &calls, const Limits limits) {
    auto result = std::make_unique<Inference>(limits);
    for (const auto &call : calls.calls) {
        result->callees.emplace(&call.caller.module->syntax->expression(call.expression), call.callee);
    }
    // Local inputs grow from their calls and a fun F/A reads its function's summary as far as it is known: passes
    // repeat until the inputs stop changing and each fun read its function's final result.
    prepare_inputs(*result, calls);
    for (std::size_t pass = 0; pass < PASSES; ++pass) {
        infer_pass(*result, calls);
        const bool changed = gather_inputs(*result, calls, pass >= JOIN_ROUNDS);
        if (!changed && settled(*result)) {
            return result;
        }
    }
    for (auto &[function, inputs] : result->inputs) {
        (void)function;
        std::ranges::fill(inputs, result->graph.exhausted());
    }
    result->opaque_funs = true;
    infer_pass(*result, calls);
    return result;
}
} // namespace clause::semantic::types
