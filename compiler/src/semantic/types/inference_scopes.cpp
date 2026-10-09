#include "inference_scopes.hpp"
#include "../capabilities.hpp"
#include "inference_narrowing.hpp"
#include "lattice.hpp"

namespace clause::semantic::types {
namespace {
// A clause's guard: optional for case, receive and fun clauses, always present for if clauses.
const ast::GuardSyntax *guard_of(const std::optional<ast::GuardSyntax> &guard) { return guard ? &*guard : nullptr; }

const ast::GuardSyntax *guard_of(const ast::GuardSyntax &guard) { return &guard; }

// Add a guard's tests to frames in evaluation order.
void push_guard(const ast::GuardSyntax *guard, std::vector<Frame> &frames) {
    for (const auto &alternative : guard ? guard->alternatives : std::vector<ast::GuardConjunction>{}) {
        for (const auto &test : alternative.tests) {
            frames.push_back({test});
        }
    }
}

// Frames in evaluation order, pushed onto the walk's stack in reverse at the end.
class Order final {
  public:
    explicit Order(const Module &module) : module_(module) {}

    void visit(const ast::ExprId &id) { frames_.push_back({id}); }

    void visit(const std::vector<ast::ExprId> &ids) {
        for (const auto &id : ids) {
            visit(id);
        }
    }

    void step(const ast::ExprId &id, const Step step, const std::size_t clause = 0) {
        frames_.push_back({id, step, clause});
    }

    // Guard tests of every alternative.
    void guard(const ast::GuardSyntax *guard) {
        for (const auto &alternative : guard ? guard->alternatives : std::vector<ast::GuardConjunction>{}) {
            visit(alternative.tests);
        }
    }

    // Each clause of a case, if, receive or fun: entered, guarded, evaluated and left.
    template <typename Clauses> void clauses(const ast::ExprId &id, const Clauses &clauses) {
        for (std::size_t index = 0; index < clauses.size(); ++index) {
            step(id, Step::enter, index);
            guard(guard_of(clauses[index].guard));
            step(id, Step::guarded, index);
            visit(clauses[index].body);
            step(id, Step::leave, index);
        }
    }

    void push(std::vector<Frame> &pending) const { pending.insert(pending.end(), frames_.rbegin(), frames_.rend()); }

    const Module &module() const { return module_; }

  private:
    const Module &module_;
    std::vector<Frame> frames_;
};

// The frames of a case, if or fun; false for other expressions.
bool selection(Order &order, const ast::ExprId &id, const ast::ExprValue &value) {
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        order.visit(selection->value);
        order.clauses(id, selection->clauses);
        return true;
    }
    if (const auto *choice = std::get_if<ast::IfExpression>(&value)) {
        order.clauses(id, choice->clauses);
        return true;
    }
    if (const auto *clauses = fun_clauses(value)) {
        order.clauses(id, *clauses);
        return true;
    }
    return false;
}

// The frames of a receive (its timeout first, its after body last) or of andalso; false for other expressions.
bool waiting(Order &order, const ast::ExprId &id, const ast::ExprValue &value) {
    if (const auto *receive = std::get_if<ast::ReceiveExpression>(&value)) {
        if (receive->after) {
            order.visit(receive->after->timeout);
        }
        order.clauses(id, receive->clauses);
        if (receive->after) {
            order.step(id, Step::save);
            order.visit(receive->after->body);
            order.step(id, Step::restore);
        }
        return true;
    }
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    if (binary && binary->operation == ast::BinaryOperator::and_also) {
        // The right operand runs only when the left one is true.
        order.visit(binary->left);
        order.step(binary->left, Step::assume);
        order.visit(binary->right);
        order.step(id, Step::restore);
        return true;
    }
    return false;
}

// The frames of a catch or try: nothing narrowed inside them holds after them or in a handler; false for others.
bool guarded(Order &order, const ast::ExprId &id, const ast::ExprValue &value) {
    if (const auto *caught = std::get_if<ast::CatchExpression>(&value)) {
        order.step(id, Step::save);
        order.visit(caught->expression);
        order.step(id, Step::restore);
        return true;
    }
    const auto *attempt = std::get_if<ast::TryExpression>(&value);
    if (!attempt) {
        return false;
    }
    order.step(id, Step::save);
    order.visit(attempt->body);
    for (const auto &clause : branch_clauses(value)) {
        order.step(id, Step::reset);
        order.guard(clause.guard);
        order.visit(*clause.body);
    }
    if (attempt->after) {
        order.step(id, Step::reset);
        order.visit(*attempt->after);
    }
    order.step(id, Step::restore);
    return true;
}

// The frames of a comprehension: generator inputs, filters (each narrowing what follows it) and templates; false
// for other expressions.
bool comprehension(Order &order, const ast::ExprId &id, const ast::ExprValue &value) {
    const auto *qualifiers = comprehension_qualifiers(value);
    if (!qualifiers) {
        return false;
    }
    order.step(id, Step::save);
    for (const auto &qualifier : *qualifiers) {
        for (const auto &simple : zipped(qualifier)) {
            if (const auto input = generator_input(simple)) {
                order.visit(*input);
            } else {
                const auto &filter = std::get<ast::FilterQualifier>(simple.value).expression;
                order.visit(filter);
                order.step(filter, Step::assume);
            }
        }
    }
    order.visit(comprehension_templates(value));
    order.step(id, Step::restore);
    return true;
}

// The fact of an expression the walk recorded; term() for one it did not.
Id recorded(const BindingFacts &bindings, const ast::ExprId &id) {
    const auto found = bindings.inference.expressions.find(&bindings.function.module->syntax->expression(id));
    return found == bindings.inference.expressions.end() ? bindings.inference.graph.top() : found->second.type;
}

// The category the previous clause left out of value `position` (an argument, or 0 for a case's scrutinee): when
// all its patterns were plain variables and its whole guard one type test on the variable at `position`, it can
// only have failed on that test.
template <typename Clause>
std::optional<Id> left_out(BindingFacts &bindings, const Clause &previous, const std::vector<ast::ExprId> &patterns,
                           const std::size_t position) {
    const auto &syntax = *bindings.function.module->syntax;
    const bool plain = std::ranges::all_of(patterns, [&](const ast::ExprId &pattern) {
        return std::holds_alternative<ast::Variable>(syntax.expression(ungroup(syntax, pattern)).value);
    });
    const auto tested = plain && previous.guard ? single_test(bindings, *previous.guard) : std::nullopt;
    if (!tested || position >= patterns.size() || variable(bindings, patterns[position]) != tested->first) {
        return std::nullopt;
    }
    return tested->second;
}

// The roots of a clause's argument patterns.
std::vector<ast::ExprId> argument_roots(const ast::Module &syntax, const ast::FunctionClause &clause) {
    std::vector<ast::ExprId> result;
    result.reserve(clause.arguments.size());
    for (const auto &argument : clause.arguments) {
        result.push_back(pattern_root(syntax, argument));
    }
    return result;
}

// Narrow a value by a clause's pattern: publish the pattern's variables, narrow a variable the value was read from,
// and mark the clause impossible when nothing is left.
Id match(BindingFacts &bindings, const ast::ExprId &pattern, const Fact value, const std::vector<ast::ExprId> *body,
         std::size_t &work) {
    Lattice lattice(bindings.inference.graph);
    const auto narrowed = lattice.meet(value.type, pattern_shape(bindings, pattern));
    if (narrowed == lattice.graph().bottom() && value.type != narrowed) {
        bindings.impossible.insert(body);
    }
    bindings.publish(pattern, {narrowed, value.argument}, work);
    return narrowed;
}

// Enter a case clause: its pattern narrows the scrutinee, and a true pattern assumes a test scrutinee.
void enter_case(BindingFacts &bindings, const ast::CaseExpression &selection, const std::size_t index,
                std::size_t &work) {
    const auto &syntax = *bindings.function.module->syntax;
    const auto &clause = selection.clauses[index];
    Lattice lattice(bindings.inference.graph);
    auto value = recorded(bindings, selection.value);
    if (index > 0) {
        const auto &previous = selection.clauses[index - 1];
        if (const auto removed = left_out(bindings, previous, {pattern_root(syntax, previous.pattern)}, 0)) {
            value = lattice.subtract(value, *removed);
        }
    }
    const auto pattern = pattern_root(syntax, clause.pattern);
    const auto narrowed = match(bindings, pattern, {value}, &clause.body, work);
    bindings.narrow(selection.value, narrowed);
    const auto *atom = std::get_if<ast::Atom>(&syntax.expression(ungroup(syntax, pattern)).value);
    if (atom && atom->name == U"true" && !assume(bindings, selection.value)) {
        bindings.impossible.insert(&clause.body);
    }
}

// Enter a clause of a case, receive or fun: its patterns narrow what they match.
void enter(BindingFacts &bindings, const ast::ExprValue &value, const std::size_t index, std::size_t &work) {
    const auto &syntax = *bindings.function.module->syntax;
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        enter_case(bindings, *selection, index, work);
    } else if (const auto *receive = std::get_if<ast::ReceiveExpression>(&value)) {
        const auto &clause = receive->clauses[index];
        match(bindings, pattern_root(syntax, clause.pattern), {bindings.inference.graph.top()}, &clause.body, work);
    } else if (const auto *clauses = fun_clauses(value)) {
        for (const auto &pattern : argument_roots(syntax, clauses->at(index))) {
            match(bindings, pattern, {bindings.inference.graph.top()}, &clauses->at(index).body, work);
        }
    }
}

// A clause's guard and body, by construct.
std::pair<const ast::GuardSyntax *, const std::vector<ast::ExprId> *> clause_parts(const ast::ExprValue &value,
                                                                                   const std::size_t index) {
    const auto clauses = branch_clauses(value);
    if (const auto *fun = fun_clauses(value)) {
        const auto &clause = fun->at(index);
        return {clause.guard ? &*clause.guard : nullptr, &clause.body};
    }
    return {clauses.at(index).guard, clauses.at(index).body};
}

// The function clauses of the walked function.
const std::vector<ast::FunctionClause> &heads(const BindingFacts &bindings) {
    const auto &syntax = *bindings.function.module->syntax;
    return std::get<ast::Function>(syntax.form(bindings.function.function->form).value).clauses;
}

// Enter a function clause: its head patterns narrow the inputs, which the previous clause's single type test may
// already have narrowed.
void enter_head(BindingFacts &bindings, const std::size_t index, std::size_t &work) {
    const auto &syntax = *bindings.function.module->syntax;
    const auto &clauses = heads(bindings);
    const auto patterns = argument_roots(syntax, clauses[index]);
    Lattice lattice(bindings.inference.graph);
    bindings.entry.assign(patterns.size(), bindings.inference.graph.top());
    for (std::size_t position = 0; position < patterns.size(); ++position) {
        auto input = bindings.inputs.at(position);
        if (index > 0) {
            const auto previous = argument_roots(syntax, clauses[index - 1]);
            if (const auto removed = left_out(bindings, clauses[index - 1], previous, position)) {
                input = lattice.subtract(input, *removed);
            }
        }
        bindings.entry[position] = match(bindings, patterns[position], {input, position}, &clauses[index].body, work);
    }
}

// After a function clause's guard: add each argument's fact (a plain variable's narrowed by the guard) to the
// entry domain, unless the clause can never run.
void guarded_head(BindingFacts &bindings, const std::size_t index) {
    const auto &syntax = *bindings.function.module->syntax;
    const auto &clause = heads(bindings)[index];
    if (clause.guard && !assume_guard(bindings, *clause.guard)) {
        bindings.impossible.insert(&clause.body);
    }
    if (bindings.impossible.contains(&clause.body)) {
        return;
    }
    Lattice lattice(bindings.inference.graph);
    const auto patterns = argument_roots(syntax, clause);
    for (std::size_t position = 0; position < patterns.size(); ++position) {
        const auto identity = variable(bindings, patterns[position]);
        const auto found = identity ? bindings.values.find(*identity) : bindings.values.end();
        const auto fact = found == bindings.values.end() ? bindings.entry.at(position) : found->second.type;
        bindings.domain.at(position) = lattice.join(bindings.domain.at(position), fact);
    }
}

// Restore the facts saved when the scope began, keeping them saved when `keep`.
void restore(BindingFacts &bindings, const bool keep) {
    bindings.values = bindings.saved.back();
    if (!keep) {
        bindings.saved.pop_back();
    }
}

// Narrow by a clause's guard.
void guard_clause(BindingFacts &bindings, const ast::ExprValue &value, const std::size_t index) {
    const auto [guard, body] = clause_parts(value, index);
    if (guard && !assume_guard(bindings, *guard)) {
        bindings.impossible.insert(body);
    }
}
} // namespace

void expand(const Module &module, const ast::ExprId &id, std::vector<Frame> &pending) {
    const auto &value = module.syntax->expression(id).value;
    pending.push_back({id, Step::ready});
    Order order(module);
    if (!selection(order, id, value) && !waiting(order, id, value) && !guarded(order, id, value) &&
        !comprehension(order, id, value)) {
        order.visit(expression_children(module, module.syntax->expression(id)));
    }
    order.push(pending);
}

void expand_heads(const ast::Function &definition, std::vector<Frame> &pending) {
    std::vector<Frame> frames;
    for (std::size_t index = 0; index < definition.clauses.size(); ++index) {
        const auto &clause = definition.clauses[index];
        frames.push_back({clause.body.front(), Step::enter_head, index});
        push_guard(guard_of(clause.guard), frames);
        frames.push_back({clause.body.front(), Step::guarded_head, index});
        for (const auto &expression : clause.body) {
            frames.push_back({expression});
        }
        frames.push_back({clause.body.front(), Step::leave_head, index});
    }
    pending.insert(pending.end(), frames.rbegin(), frames.rend());
}

void scope_step(BindingFacts &bindings, const Frame &frame, std::size_t &work) {
    using Handler = void (*)(BindingFacts &, const Frame &, std::size_t &);
    static const std::map<Step, Handler> HANDLERS{
        {Step::save, [](BindingFacts &b, const Frame &, std::size_t &) { b.saved.push_back(b.values); }},
        {Step::restore, [](BindingFacts &b, const Frame &, std::size_t &) { restore(b, false); }},
        {Step::reset, [](BindingFacts &b, const Frame &, std::size_t &) { restore(b, true); }},
        {Step::leave, [](BindingFacts &b, const Frame &, std::size_t &) { restore(b, false); }},
        {Step::leave_head, [](BindingFacts &b, const Frame &, std::size_t &) { restore(b, false); }},
        {Step::assume,
         [](BindingFacts &b, const Frame &f, std::size_t &) {
             b.saved.push_back(b.values);
             (void)assume(b, f.expression);
         }},
        {Step::enter,
         [](BindingFacts &b, const Frame &f, std::size_t &work) {
             b.saved.push_back(b.values);
             enter(b, b.function.module->syntax->expression(f.expression).value, f.clause, work);
         }},
        {Step::guarded,
         [](BindingFacts &b, const Frame &f, std::size_t &) {
             guard_clause(b, b.function.module->syntax->expression(f.expression).value, f.clause);
         }},
        {Step::enter_head,
         [](BindingFacts &b, const Frame &f, std::size_t &work) {
             b.saved.push_back(b.values);
             enter_head(b, f.clause, work);
         }},
        {Step::guarded_head, [](BindingFacts &b, const Frame &f, std::size_t &) { guarded_head(b, f.clause); }}};
    HANDLERS.at(frame.step)(bindings, frame, work);
}
} // namespace clause::semantic::types
