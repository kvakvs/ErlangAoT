#include "../semantic/capabilities.hpp"
#include "../semantic/records.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <array>
#include <erlang_aot/abi/calls.hpp>
#include <llvm/Transforms/Utils/SSAUpdater.h>

namespace erlang_aot::codegen {
namespace {
enum class Action : std::uint8_t { enter, value, lazy_left, lazy_right, record_field, case_select, case_clause_end };

struct Visit {
    // Explicit actions keep ordinary and conditionally reached operands off the host stack.
    ast::ExprId id;
    Action action = Action::enter;
    // Record-field actions capture one completed evaluation before a reused initializer can run again;
    // case clause actions name their clause here.
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
    // Every clause matches the same scrutinee and starts from the bindings visible before the case.
    llvm::Value *value;
    std::map<semantic::BindingId, llvm::Value *> bindings;
    // The merge block is inserted after the last clause; completed clauses collect their incoming edges.
    llvm::BasicBlock *merge;
    // The mismatch continuation of the current clause starts the next one, or raises case_clause.
    llvm::BasicBlock *next = nullptr;
    std::vector<CaseIncoming> incoming = {};
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

// A case evaluates its scrutinee first; clause selection starts once that value exists.
bool case_enter(ExpressionLowering &state, const ast::ExprId &id, std::vector<Visit> &pending) {
    const auto *selection = std::get_if<ast::CaseExpression>(&state.module.syntax->expression(id).value);
    if (!selection) {
        return false;
    }
    pending.push_back({id, Action::case_select});
    pending.push_back({selection->value});
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

// The reached RHS may return any admitted term; only its enclosing consumer imposes another boolean check.
llvm::Value *right(ExpressionLowering &state, const ast::Expression &expression, const LazyJoin &join) {
    const auto &binary = std::get<ast::BinaryExpression>(expression.value);
    locate_source(state.builder, *state.module.syntax, expression.source);
    auto *value = state.values.at(&state.module.syntax->expression(binary.right));
    auto *predecessor = state.builder.GetInsertBlock();
    state.builder.CreateBr(join.merge);
    state.builder.SetInsertPoint(join.merge);
    // SSA formation inspects successors, so terminate the unfinished join until its surrounding expression resumes.
    auto *boundary = state.builder.CreateUnreachable();
    llvm::SmallVector<llvm::PHINode *, 2> phis;
    llvm::SSAUpdater updater(&phis);
    updater.Initialize(state.word, "lazy.value");
    updater.AddAvailableValue(join.short_path, join.left);
    updater.AddAvailableValue(predecessor, value);
    auto *result = updater.GetValueInMiddleOfBlock(join.merge);
    for (auto *phi : phis) {
        phi->setDebugLoc(state.builder.getCurrentDebugLocation());
    }
    boundary->eraseFromParent();
    return result;
}

struct Walk {
    // Borrow the function state while retaining bounded AST-sized scheduling and unfinished joins.
    ExpressionLowering &state;
    std::vector<Visit> pending;
    std::map<const ast::Expression *, LazyJoin> joins;
    std::map<const ast::Expression *, CaseJoin> cases;

    // Open the case join and try its first clause.
    void select(const ast::ExprId &id) {
        const auto &expression = state.module.syntax->expression(id);
        const auto &selection = std::get<ast::CaseExpression>(expression.value);
        auto *merge = llvm::BasicBlock::Create(state.entry.getContext(), "case.join");
        auto *value = state.values.at(&state.module.syntax->expression(selection.value));
        cases.try_emplace(&expression, value, state.bindings, merge);
        start_clause(id, 0);
    }

    // Match one clause pattern and guard from the case's entry bindings, then schedule its body.
    void start_clause(const ast::ExprId &id, const std::size_t index) {
        const auto &expression = state.module.syntax->expression(id);
        const auto &clause = std::get<ast::CaseExpression>(expression.value).clauses.at(index);
        auto &join = cases.at(&expression);
        auto &context = state.entry.getContext();
        state.bindings = join.bindings;
        auto *body = llvm::BasicBlock::Create(context, "case.body", &state.entry);
        auto *guard = clause.guard ? llvm::BasicBlock::Create(context, "case.guard", &state.entry) : body;
        join.next = llvm::BasicBlock::Create(context, "case.next", &state.entry);
        const auto plan = body_pattern_plan(state, semantic::pattern_root(*state.module.syntax, clause.pattern));
        lower_match_plan(state, plan, std::array{join.value}, guard, join.next);
        if (clause.guard) {
            state.builder.SetInsertPoint(guard);
            lower_guard(state, *clause.guard, {.success = body, .rejection = join.next});
        }
        state.builder.SetInsertPoint(body);
        pending.push_back({id, Action::case_clause_end, index});
        for (auto child = clause.body.rbegin(); child != clause.body.rend(); ++child) {
            pending.push_back({*child});
        }
    }

    // Record a completed clause's edge to the join, then try the next clause or finish the case.
    void clause_end(const Visit &visit) {
        const auto &expression = state.module.syntax->expression(visit.id);
        const auto &selection = std::get<ast::CaseExpression>(expression.value);
        auto &join = cases.at(&expression);
        const auto &last = state.module.syntax->expression(selection.clauses.at(visit.field).body.back());
        CaseIncoming completed{state.builder.GetInsertBlock(), {state.values.at(&last)}};
        for (const auto &identity : state.function.exports.at(&expression)) {
            completed.values.push_back(state.bindings.at(identity));
        }
        join.incoming.push_back(std::move(completed));
        state.builder.CreateBr(join.merge);
        state.builder.SetInsertPoint(join.next);
        if (visit.field + 1 < selection.clauses.size()) {
            start_clause(visit.id, visit.field + 1);
            return;
        }
        finish_case(expression, join);
        cases.erase(&expression);
    }

    // Raise {case_clause, Value} when no clause matched, then join clause values and exported bindings.
    void finish_case(const ast::Expression &expression, CaseJoin &join) {
        if (join.next->use_empty()) {
            state.builder.ClearInsertionPoint();
            join.next->eraseFromParent();
        } else {
            raise_reason(state, abi::v1::ErrorReason::case_clause, join.value);
        }
        join.merge->insertInto(&state.entry);
        state.builder.SetInsertPoint(join.merge);
        locate_source(state.builder, *state.module.syntax, expression.source);
        // SSA formation inspects successors, so terminate the join until the surrounding expression resumes.
        auto *boundary = state.builder.CreateUnreachable();
        auto *result = merged(state, join, 0);
        state.bindings = std::move(join.bindings);
        const auto &exports = state.function.exports.at(&expression);
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
            enter(state, visit.id, pending);
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
        case Action::case_select:
            select(visit.id);
            break;
        case Action::case_clause_end:
            clause_end(visit);
            break;
        }
    }
};
} // namespace

llvm::Value *lower_body(ExpressionLowering &state, const ast::ExprId &root) {
    Walk walk{state, {{root}}, {}, {}};
    while (!walk.pending.empty()) {
        const auto visit = walk.pending.back();
        walk.pending.pop_back();
        walk.visit(visit);
    }
    return state.values.at(&state.module.syntax->expression(root));
}
} // namespace erlang_aot::codegen
