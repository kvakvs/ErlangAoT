#include "../semantic/capabilities.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <llvm/Transforms/Utils/SSAUpdater.h>

namespace erlang_aot::codegen {
namespace {
enum class Action : std::uint8_t { enter, value, lazy_left, lazy_right };

struct Visit {
    // Explicit actions keep ordinary and conditionally reached operands off the host stack.
    ast::ExprId id;
    Action action = Action::enter;
};

struct LazyJoin {
    // Preserve the short-path term and its predecessor until the reached RHS can complete the word PHI.
    llvm::Value *left;
    llvm::BasicBlock *short_path;
    llvm::BasicBlock *merge;
};

// Lazy syntax has an executable RHS block; strict boolean operators use ordinary eager value scheduling.
const ast::BinaryExpression *lazy(const ast::ExprValue &value) {
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    return binary && (binary->operation == ast::BinaryOperator::and_also ||
                      binary->operation == ast::BinaryOperator::or_else)
               ? binary
               : nullptr;
}

// Reverse-push eager children to preserve source order; schedule only the left operand for lazy syntax.
void enter(ExpressionLowering &state, const ast::ExprId &id, std::vector<Visit> &pending) {
    const auto &expression = state.module.syntax->expression(id);
    if (const auto *binary = lazy(expression.value)) {
        pending.push_back({id, Action::lazy_left});
        pending.push_back({binary->left});
        return;
    }
    pending.push_back({id, Action::value});
    if (semantic::integer_literal(*state.module.syntax, id, state.word->getBitWidth())) {
        return;
    }
    const auto children = semantic::expression_children(expression);
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

    // Every action either emits a visited value or schedules the next source-ordered operand.
    void visit(const Visit &visit) {
        const auto &expression = state.module.syntax->expression(visit.id);
        switch (visit.action) {
        case Action::enter:
            enter(state, visit.id, pending);
            break;
        case Action::value:
            state.values.emplace(&expression, lower_value(state, visit.id));
            break;
        case Action::lazy_left: {
            const auto &binary = std::get<ast::BinaryExpression>(expression.value);
            joins.emplace(&expression, left(state, expression, binary));
            pending.push_back({visit.id, Action::lazy_right});
            pending.push_back({binary.right});
            break;
        }
        case Action::lazy_right:
            state.values.emplace(&expression, right(state, expression, joins.at(&expression)));
            joins.erase(&expression);
            break;
        }
    }
};
} // namespace

llvm::Value *lower_body(ExpressionLowering &state, const ast::ExprId &root) {
    Walk walk{state, {{root}}, {}};
    while (!walk.pending.empty()) {
        const auto visit = walk.pending.back();
        walk.pending.pop_back();
        walk.visit(visit);
    }
    return state.values.at(&state.module.syntax->expression(root));
}
} // namespace erlang_aot::codegen
