#include "../semantic/capabilities.hpp"
#include "../semantic/records.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <array>
#include <erlang_aot/abi/calls.hpp>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <span>

namespace erlang_aot::codegen {
namespace {
enum class Action : std::uint8_t {
    enter,
    value,
    lazy_left,
    lazy_right,
    record_field,
    case_select,
    case_clause_end,
    catch_end,
    try_body_end,
    after_end,
    after_raise_end
};

struct Visit {
    // Explicit actions keep ordinary and conditionally reached operands off the host stack.
    ast::ExprId id;
    Action action = Action::enter;
    // Record-field actions capture one completed evaluation before a reused initializer can run again;
    // case and if clause actions name their clause here.
    std::size_t field = 0;
    std::optional<ast::ExprId> child = {};
};

struct LazyJoin {
    // Preserve the short-path term and its predecessor until the reached RHS can complete the word PHI.
    llvm::Value *left;
    llvm::BasicBlock *short_path;
    llvm::BasicBlock *merge;
};

struct CaseIncoming {
    // One completed clause: its final block, then its value followed by the case's exported bindings there.
    llvm::BasicBlock *block;
    std::vector<llvm::Value *> values;
};

struct CaseJoin {
    // Every case clause matches the same scrutinee (null for if, the body value for a try's of clauses); clauses
    // start from the bindings before the branch (catch clauses from those before the try).
    llvm::Value *value;
    std::map<semantic::BindingId, llvm::Value *> bindings;
    // The merge block is inserted after the last clause; completed clauses collect their incoming edges.
    llvm::BasicBlock *merge;
    // The mismatch continuation of the current clause starts the next one, or raises case_clause/if_clause.
    llvm::BasicBlock *next = nullptr;
    std::vector<CaseIncoming> incoming = {};
    // A try's catch clauses match the caught class atom and reason.
    std::array<llvm::Value *, 2> exception = {};
};

struct ProtectedScope {
    // The enclosing handler and error exits come back once the protected catch expression or try body completes.
    llvm::BasicBlock *outer;
    llvm::BasicBlock *bad_argument;
    llvm::BasicBlock *bad_arithmetic;
    // Names bound inside a catch or try are unsafe afterwards, so the bindings before it are restored.
    std::map<semantic::BindingId, llvm::Value *> bindings;
    // Failures inside the protected part branch here; it is erased when nothing inside can fail.
    llvm::BasicBlock *handler;
};

struct AfterPath {
    // The try value outlives the normal-path after body, whose end `resume` continues once the raising path is done.
    llvm::Value *result;
    llvm::BasicBlock *resume = nullptr;
    // The exception taken by the after handler on the raising path.
    std::array<llvm::Value *, 2> exception = {};
};

// Merge one per-clause value at the join; SSAUpdater adds a PHI only when the clauses disagree.
llvm::Value *merged(const ExpressionLowering &state, const CaseJoin &join, const std::size_t slot) {
    llvm::SmallVector<llvm::PHINode *, 2> phis;
    llvm::SSAUpdater updater(&phis);
    updater.Initialize(state.word, slot == 0 ? "case.value" : "case.export");
    for (const auto &completed : join.incoming) {
        updater.AddAvailableValue(completed.block, completed.values.at(slot));
    }
    auto *value = updater.GetValueInMiddleOfBlock(join.merge);
    for (auto *phi : phis) {
        phi->setDebugLoc(state.builder.getCurrentDebugLocation());
    }
    return value;
}

// Lazy syntax has an executable RHS block; strict boolean operators use ordinary eager value scheduling.
const ast::BinaryExpression *lazy(const ast::ExprValue &value) {
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    return binary && (binary->operation == ast::BinaryOperator::and_also ||
                      binary->operation == ast::BinaryOperator::or_else)
               ? binary
               : nullptr;
}

// Literal binary strings expand directly to segment scalars; only their size expression needs evaluation.
std::vector<ast::ExprId> binary_children(const ast::Module &syntax, const ast::Bitstring &binary) {
    std::vector<ast::ExprId> children;
    for (const auto &segment : binary.segments) {
        const auto &value = syntax.expression(semantic::ungroup(syntax, segment.value)).value;
        if (!std::holds_alternative<ast::StringLiteral>(value)) {
            children.push_back(segment.value);
        }
        if (segment.size) {
            children.push_back(*segment.size);
        }
    }
    return children;
}

// Reverse-push eager children to preserve source order; schedule only the left operand for lazy syntax.
bool record_enter(ExpressionLowering &state, const ast::ExprId &id, std::vector<Visit> &pending) {
    const auto &expression = state.module.syntax->expression(id);
    const auto *record = std::get_if<ast::RecordExpression>(&expression.value);
    if (!record) {
        return false;
    }
    const auto fields = semantic::record_values(state.module, *record, false);
    state.record_values.insert_or_assign(&expression, std::vector<llvm::Value *>(fields.size()));
    pending.push_back({id, Action::value});
    for (std::size_t i = fields.size(); i != 0; --i) {
        pending.push_back({id, Action::record_field, i - 1, fields[i - 1]});
        if (fields[i - 1]) {
            pending.push_back({*fields[i - 1]});
        }
    }
    return true;
}

// A case evaluates its scrutinee first and clause selection starts once that value exists; an if selects at once.
bool case_enter(ExpressionLowering &state, const ast::ExprId &id, std::vector<Visit> &pending) {
    const auto &value = state.module.syntax->expression(id).value;
    if (!std::holds_alternative<ast::CaseExpression>(value) && !std::holds_alternative<ast::IfExpression>(value)) {
        return false;
    }
    pending.push_back({id, Action::case_select});
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        pending.push_back({selection->value});
    }
    return true;
}

// Reverse-push eager children to preserve source order; schedule only the left operand for lazy syntax.
void enter(ExpressionLowering &state, const ast::ExprId &id, std::vector<Visit> &pending) {
    const auto &expression = state.module.syntax->expression(id);
    if (const auto *binary = lazy(expression.value)) {
        pending.push_back({id, Action::lazy_left});
        pending.push_back({binary->left});
        return;
    }
    if (record_enter(state, id, pending) || case_enter(state, id, pending)) {
        return;
    }
    pending.push_back({id, Action::value});
    if (semantic::integer_literal(*state.module.syntax, id, state.word->getBitWidth())) {
        return;
    }
    const auto *binary = std::get_if<ast::Bitstring>(&expression.value);
    const auto children = binary ? binary_children(*state.module.syntax, *binary)
                                 : semantic::expression_children(state.module, expression);
    for (auto child = children.rbegin(); child != children.rend(); ++child) {
        pending.push_back({*child});
    }
}

// The left operand must be boolean; its errors reject the enclosing guard instead of becoming false.
LazyJoin left(ExpressionLowering &state, const ast::Expression &expression, const ast::BinaryExpression &binary) {
    locate_source(state.builder, *state.module.syntax, expression.source);
    auto *value = state.values.at(&state.module.syntax->expression(binary.left));
    value = lower_immediate(state, abi::v1::ImmediateOperation::boolean_check, value);
    auto *truth = lower_atom(state, ast::Atom{U"true"});
    auto *test = lower_exact(state, value, truth);
    auto *short_path = state.builder.GetInsertBlock();
    auto *merge = llvm::BasicBlock::Create(state.entry.getContext(), "lazy.join", &state.entry);
    auto *right = llvm::BasicBlock::Create(state.entry.getContext(), "lazy.right", &state.entry);
    if (binary.operation == ast::BinaryOperator::and_also) {
        state.builder.CreateCondBr(test, right, merge);
    } else {
        state.builder.CreateCondBr(test, merge, right);
    }
    state.builder.SetInsertPoint(right);
    return {value, short_path, merge};
}

struct Incoming {
    // One predecessor edge of a two-way join and the value it carries.
    llvm::BasicBlock *block;
    llvm::Value *value;
};

// Join two edges at `merge` (the insertion block); SSAUpdater adds a PHI only when the values differ.
llvm::Value *join_two(ExpressionLowering &state, llvm::BasicBlock *merge, const char *name, const Incoming first,
                      const Incoming second) {
    // SSA formation inspects successors, so terminate the unfinished join until its surrounding expression resumes.
    auto *boundary = state.builder.CreateUnreachable();
    llvm::SmallVector<llvm::PHINode *, 2> phis;
    llvm::SSAUpdater updater(&phis);
    updater.Initialize(state.word, name);
    updater.AddAvailableValue(first.block, first.value);
    updater.AddAvailableValue(second.block, second.value);
    auto *result = updater.GetValueInMiddleOfBlock(merge);
    for (auto *phi : phis) {
        phi->setDebugLoc(state.builder.getCurrentDebugLocation());
    }
    boundary->eraseFromParent();
    return result;
}

// The reached RHS may return any admitted term; only its enclosing consumer imposes another boolean check.
llvm::Value *right(ExpressionLowering &state, const ast::Expression &expression, const LazyJoin &join) {
    const auto &binary = std::get<ast::BinaryExpression>(expression.value);
    locate_source(state.builder, *state.module.syntax, expression.source);
    auto *value = state.values.at(&state.module.syntax->expression(binary.right));
    auto *predecessor = state.builder.GetInsertBlock();
    state.builder.CreateBr(join.merge);
    state.builder.SetInsertPoint(join.merge);
    return join_two(state, join.merge, "lazy.value", {join.short_path, join.left}, {predecessor, value});
}

struct Walk {
    // Borrow the function state while retaining bounded AST-sized scheduling and unfinished joins.
    ExpressionLowering &state;
    std::vector<Visit> pending;
    std::map<const ast::Expression *, LazyJoin> joins;
    std::map<const ast::Expression *, CaseJoin> cases;
    std::map<const ast::Expression *, ProtectedScope> catches;
    // A try's after protection encloses its body and clauses; its handler runs the after body on the raising path.
    std::map<const ast::Expression *, ProtectedScope> afters;
    std::map<const ast::Expression *, AfterPath> after_paths;

    // Save the enclosing handler and error exits in `scopes`; failures in what follows reach a new handler instead.
    void protect(std::map<const ast::Expression *, ProtectedScope> &scopes, const ast::Expression &expression,
                 const char *name) {
        auto *handler = llvm::BasicBlock::Create(state.entry.getContext(), name, &state.entry);
        scopes.try_emplace(&expression, state.handler, state.bad_argument, state.bad_arithmetic, state.bindings,
                           handler);
        // Shared error exits created outside the protected part would bypass its handler.
        state.handler = handler;
        state.bad_argument = nullptr;
        state.bad_arithmetic = nullptr;
    }

    // Restore the enclosing handler and error exits once a protected part completes.
    void restore(const ProtectedScope &scope) {
        state.handler = scope.outer;
        state.bad_argument = scope.bad_argument;
        state.bad_arithmetic = scope.bad_arithmetic;
    }

    // Protect a catch expression: failures inside it reach a fresh handler instead of the enclosing exit.
    bool open_catch(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto *guarded = std::get_if<ast::CatchExpression>(&expression.value);
        if (!guarded) {
            return false;
        }
        protect(catches, expression, "catch.handler");
        pending.push_back({id, Action::catch_end});
        pending.push_back({guarded->expression});
        return true;
    }

    // Join the protected value with the caught exception value, then restore the enclosing scope.
    void close_catch(const ast::Expression &expression) {
        auto node = catches.extract(&expression);
        auto &scope = node.mapped();
        auto *handler = scope.handler;
        restore(scope);
        state.bindings = std::move(scope.bindings);
        const auto &guarded = std::get<ast::CatchExpression>(expression.value);
        auto *value = state.values.at(&state.module.syntax->expression(guarded.expression));
        if (handler->use_empty()) {
            handler->eraseFromParent();
            state.values.insert_or_assign(&expression, value);
            return;
        }
        locate_source(state.builder, *state.module.syntax, expression.source);
        auto *normal = state.builder.GetInsertBlock();
        auto *merge = llvm::BasicBlock::Create(state.entry.getContext(), "catch.join", &state.entry);
        state.builder.CreateBr(merge);
        state.builder.SetInsertPoint(handler);
        auto *caught = lower_catch(state);
        auto *handled = state.builder.GetInsertBlock();
        state.builder.CreateBr(merge);
        state.builder.SetInsertPoint(merge);
        // Both incoming values are already rooted: the protected result and the catch service's slot.
        auto *result = join_two(state, merge, "catch.value", {normal, value}, {handled, caught});
        state.values.insert_or_assign(&expression, result);
    }

    // Protect a try body: its exceptions reach the catch clauses, which like the of clauses run outside that
    // protection; an after protection encloses the body and all clauses.
    bool open_try(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto *attempt = std::get_if<ast::TryExpression>(&expression.value);
        if (!attempt) {
            return false;
        }
        if (attempt->after) {
            protect(afters, expression, "try.after");
        }
        if (attempt->handlers) {
            protect(catches, expression, "try.handler");
        }
        pending.push_back({id, Action::try_body_end});
        for (auto body = attempt->body.rbegin(); body != attempt->body.rend(); ++body) {
            pending.push_back({*body});
        }
        return true;
    }

    // Leave the protected try body, then select an of clause on its value or join that value directly.
    void try_body_end(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto &attempt = std::get<ast::TryExpression>(expression.value);
        auto *value = state.values.at(&state.module.syntax->expression(attempt.body.back()));
        if (const auto scope = catches.find(&expression); scope != catches.end()) {
            restore(scope->second);
        }
        auto *merge = llvm::BasicBlock::Create(state.entry.getContext(), "try.join");
        auto &join = cases.try_emplace(&expression, value, state.bindings, merge).first->second;
        if (attempt.of) {
            start_clause(id, 0);
            return;
        }
        join.incoming.push_back({state.builder.GetInsertBlock(), {value}});
        state.builder.CreateBr(merge);
        handlers(id);
    }

    // Start the catch clauses from the try's handler with the bindings before the try, or finish the try when it
    // has none or nothing in its body can fail.
    void handlers(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        auto &join = cases.at(&expression);
        auto node = catches.extract(&expression);
        if (node.empty()) {
            finish(id, join);
            return;
        }
        auto &scope = node.mapped();
        join.bindings = std::move(scope.bindings);
        if (scope.handler->use_empty()) {
            scope.handler->eraseFromParent();
            state.builder.ClearInsertionPoint();
            finish(id, join);
            return;
        }
        state.builder.SetInsertPoint(scope.handler);
        locate_source(state.builder, *state.module.syntax, expression.source);
        join.exception = lower_exception(state);
        start_clause(id, semantic::first_handler(expression.value));
    }

    // Enter an expression: a catch or try opens its protected scope, every other node schedules its operands.
    void enter_node(const ast::ExprId &id) {
        if (!open_catch(id) && !open_try(id)) {
            enter(state, id, pending);
        }
    }

    // Open the case or if join and try its first clause.
    void select(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto *selection = std::get_if<ast::CaseExpression>(&expression.value);
        auto *merge = llvm::BasicBlock::Create(state.entry.getContext(), "case.join");
        auto *value = selection ? state.values.at(&state.module.syntax->expression(selection->value)) : nullptr;
        cases.try_emplace(&expression, value, state.bindings, merge);
        start_clause(id, 0);
    }

    // Match one clause pattern (case only) and guard from the entry bindings, then schedule its body.
    void start_clause(const ast::ExprId &id, const std::size_t index) {
        const auto &expression = state.module.syntax->expression(id);
        const auto clause = semantic::branch_clauses(expression.value).at(index);
        auto &join = cases.at(&expression);
        auto &context = state.entry.getContext();
        state.bindings = join.bindings;
        auto *body = llvm::BasicBlock::Create(context, "case.body", &state.entry);
        join.next = llvm::BasicBlock::Create(context, "case.next", &state.entry);
        if (clause.pattern) {
            auto *guard = clause.guard ? llvm::BasicBlock::Create(context, "case.guard", &state.entry) : body;
            match_head(clause, join, guard);
            state.builder.SetInsertPoint(guard);
        }
        if (clause.guard) {
            lower_guard(state, *clause.guard, {.success = body, .rejection = join.next});
        }
        state.builder.SetInsertPoint(body);
        pending.push_back({id, Action::case_clause_end, index});
        for (auto child = clause.body->rbegin(); child != clause.body->rend(); ++child) {
            pending.push_back({*child});
        }
    }

    // Match a clause pattern against the case value, or a catch clause's class and reason against the exception.
    void match_head(const semantic::Branch &clause, const CaseJoin &join, llvm::BasicBlock *success) {
        auto *input = join.value;
        if (clause.handler) {
            auto *reason = llvm::BasicBlock::Create(state.entry.getContext(), "catch.reason", &state.entry);
            match_class(*clause.handler, join.exception[0], reason, join.next);
            state.builder.SetInsertPoint(reason);
            input = join.exception[1];
        }
        const auto plan = body_pattern_plan(state, semantic::pattern_root(*state.module.syntax, *clause.pattern));
        lower_match_plan(state, plan, std::array{input}, success, join.next);
    }

    // An explicit class is an atom or variable pattern; an omitted class matches throw.
    void match_class(const ast::CatchClause &handler, llvm::Value *name, llvm::BasicBlock *success,
                     llvm::BasicBlock *mismatch) {
        if (handler.exception_class) {
            const auto plan = body_pattern_plan(state, *handler.exception_class);
            lower_match_plan(state, plan, std::array{name}, success, mismatch);
            return;
        }
        auto *test = lower_exact(state, name, lower_atom(state, ast::Atom{U"throw"}));
        state.builder.CreateCondBr(test, success, mismatch);
    }

    // Bindings a case or if exports; everything bound inside a try is unsafe afterwards.
    std::span<const semantic::BindingId> exported(const ast::Expression &expression) const {
        if (std::holds_alternative<ast::TryExpression>(expression.value)) {
            return {};
        }
        return state.function.exports.at(&expression);
    }

    // Record a completed clause's edge to the join, then try the next clause of the same group (case and if
    // clauses, a try's of clauses or its catch clauses) or finish the group.
    void clause_end(const Visit &visit) {
        const auto &expression = state.module.syntax->expression(visit.id);
        const auto clauses = semantic::branch_clauses(expression.value);
        auto &join = cases.at(&expression);
        const auto &last = state.module.syntax->expression(clauses.at(visit.field).body->back());
        CaseIncoming completed{state.builder.GetInsertBlock(), {state.values.at(&last)}};
        for (const auto &identity : exported(expression)) {
            completed.values.push_back(state.bindings.at(identity));
        }
        join.incoming.push_back(std::move(completed));
        state.builder.CreateBr(join.merge);
        state.builder.SetInsertPoint(join.next);
        const auto first = semantic::first_handler(expression.value);
        const auto end = visit.field < first ? first : clauses.size();
        if (visit.field + 1 < end) {
            start_clause(visit.id, visit.field + 1);
            return;
        }
        no_match(expression, join, clauses.at(visit.field));
        if (std::holds_alternative<ast::TryExpression>(expression.value) && visit.field < first) {
            handlers(visit.id);
            return;
        }
        finish(visit.id, join);
    }

    // Join the selection, then leave a try's after protection and lower its after body on the normal path.
    void finish(const ast::ExprId &id, CaseJoin &join) {
        const auto &expression = state.module.syntax->expression(id);
        finish_case(expression, join);
        cases.erase(&expression);
        if (const auto found = afters.find(&expression); found != afters.end()) {
            restore(found->second);
            state.bindings = found->second.bindings;
            after_paths.try_emplace(&expression, state.values.at(&expression));
            schedule_after(id, Action::after_end);
        }
    }

    // Lower the after body; its value is discarded and its names are unsafe afterwards.
    void schedule_after(const ast::ExprId &id, const Action action) {
        const auto &after = std::get<ast::TryExpression>(state.module.syntax->expression(id).value).after;
        if (!after) {
            return;
        }
        pending.push_back({id, action});
        for (auto body = after->rbegin(); body != after->rend(); ++body) {
            pending.push_back({*body});
        }
    }

    // Finish the normal path; when anything protected can raise, take that exception at the after handler and
    // lower the after body again before raising it.
    void after_end(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto &scope = afters.at(&expression);
        state.bindings = scope.bindings;
        if (scope.handler->use_empty()) {
            scope.handler->eraseFromParent();
            close_after(expression);
            return;
        }
        auto &path = after_paths.at(&expression);
        path.resume = state.builder.GetInsertBlock();
        state.builder.SetInsertPoint(scope.handler);
        locate_source(state.builder, *state.module.syntax, expression.source);
        path.exception = lower_exception(state);
        schedule_after(id, Action::after_raise_end);
    }

    // Raise the taken exception again after the raising-path after body, then resume the normal path.
    void after_raise_end(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto &path = after_paths.at(&expression);
        reraise(state, path.exception);
        state.builder.SetInsertPoint(path.resume);
        state.bindings = afters.at(&expression).bindings;
        close_after(expression);
    }

    // The try's value is the selected clause value, never the after body's.
    void close_after(const ast::Expression &expression) {
        state.values.insert_or_assign(&expression, after_paths.at(&expression).result);
        after_paths.erase(&expression);
        afters.erase(&expression);
    }

    // Raise {case_clause, Value}, if_clause or {try_clause, Value}, or re-raise an unmatched exception, from the last
    // clause's mismatch continuation; drop it when that clause always matches.
    void no_match(const ast::Expression &expression, const CaseJoin &join, const semantic::Branch &last) {
        if (join.next->use_empty()) {
            state.builder.ClearInsertionPoint();
            join.next->eraseFromParent();
        } else if (last.handler) {
            reraise(state, join.exception);
        } else if (std::holds_alternative<ast::TryExpression>(expression.value)) {
            raise_reason(state, abi::v1::ErrorReason::try_clause, join.value);
        } else if (join.value) {
            raise_reason(state, abi::v1::ErrorReason::case_clause, join.value);
        } else {
            raise_reason(state, abi::v1::ErrorReason::if_clause);
        }
    }

    // Join clause values and exported bindings after every clause has been tried.
    void finish_case(const ast::Expression &expression, CaseJoin &join) {
        join.merge->insertInto(&state.entry);
        state.builder.SetInsertPoint(join.merge);
        locate_source(state.builder, *state.module.syntax, expression.source);
        // SSA formation inspects successors, so terminate the join until the surrounding expression resumes.
        auto *boundary = state.builder.CreateUnreachable();
        auto *result = merged(state, join, 0);
        state.bindings = std::move(join.bindings);
        const auto exports = exported(expression);
        for (std::size_t i = 0; i < exports.size(); ++i) {
            state.bindings.insert_or_assign(exports[i], merged(state, join, i + 1));
        }
        boundary->eraseFromParent();
        state.builder.SetInsertPoint(join.merge);
        state.values.insert_or_assign(&expression, result);
        root_value(state, result);
    }

    // Every action either emits a visited value or schedules the next source-ordered operand.
    void visit(const Visit &visit) {
        const auto &expression = state.module.syntax->expression(visit.id);
        switch (visit.action) {
        case Action::enter:
            enter_node(visit.id);
            break;
        case Action::value: {
            auto *value = lower_value(state, visit.id);
            state.values.insert_or_assign(&expression, value);
            root_value(state, value);
            break;
        }
        case Action::lazy_left: {
            const auto &binary = std::get<ast::BinaryExpression>(expression.value);
            joins.emplace(&expression, left(state, expression, binary));
            pending.push_back({visit.id, Action::lazy_right});
            pending.push_back({binary.right});
            break;
        }
        case Action::lazy_right: {
            auto *value = right(state, expression, joins.at(&expression));
            state.values.insert_or_assign(&expression, value);
            root_value(state, value);
            joins.erase(&expression);
            break;
        }
        case Action::record_field:
            state.record_values.at(&expression).at(visit.field) =
                visit.child ? state.values.at(&state.module.syntax->expression(*visit.child))
                            : lower_atom(state, ast::Atom{U"undefined"});
            break;
        default:
            branch(visit);
            break;
        }
    }

    // Clause selection and exception handler actions continue a case, if, catch or try.
    void branch(const Visit &visit) {
        switch (visit.action) {
        case Action::case_select:
            select(visit.id);
            break;
        case Action::case_clause_end:
            clause_end(visit);
            break;
        case Action::catch_end:
            close_catch(state.module.syntax->expression(visit.id));
            break;
        case Action::try_body_end:
            try_body_end(visit.id);
            break;
        case Action::after_end:
            after_end(visit.id);
            break;
        default:
            after_raise_end(visit.id);
            break;
        }
    }
};
} // namespace

llvm::Value *lower_body(ExpressionLowering &state, const ast::ExprId &root) {
    Walk walk{state, {{root}}, {}, {}, {}, {}, {}};
    while (!walk.pending.empty()) {
        const auto visit = walk.pending.back();
        walk.pending.pop_back();
        walk.visit(visit);
    }
    return state.values.at(&state.module.syntax->expression(root));
}
} // namespace erlang_aot::codegen
