#pragma once
#include "../semantic/match_plan.hpp"
#include "lowering_expressions.hpp"
#include <erlang_aot/abi/calls.hpp>
#include <map>

namespace erlang_aot::codegen {
struct ExpressionLowering {
    // Borrow the current generic entry, immutable source/analysis and target-word builder.
    llvm::IRBuilder<> &builder;
    llvm::Function &entry;
    const semantic::Module &module;
    const semantic::Function &function;
    const semantic::types::Inference &inferred;
    llvm::IntegerType *word;
    // Keep source-node results for iterative, left-to-right argument evaluation.
    std::map<const ast::Expression *, llvm::Value *> values;
    // Select the source candidate without allowing bindings from another clause to enter its environment.
    std::size_t clause = 0;
    // Match definitions retain the original candidate SSA word; repeated names read this same identity.
    std::map<semantic::BindingId, llvm::Value *> bindings = {};
    // Share a terminal failure exit across calls instead of duplicating return blocks per expression.
    llvm::BasicBlock *failure = nullptr;
    // Semantic service errors reject the enclosing guard, while body errors raise badarg.
    llvm::BasicBlock *rejection = nullptr;
    llvm::BasicBlock *bad_argument = nullptr;
};

// Branch to the shared failure exit before consuming a fallible service result.
void propagate_failure(ExpressionLowering &state);
// Load one runtime-initialized atom slot; no expression evaluation interns spelling.
llvm::Value *lower_atom(ExpressionLowering &state, const ast::Atom &atom);

// Representation-aware exact comparison shares the runtime extension point with guards.
llvm::Value *lower_exact(ExpressionLowering &state, llvm::Value *left, llvm::Value *right);
// A caller supplies selection continuations; mismatch itself never mutates the error channel.
void lower_head(ExpressionLowering &state, llvm::BasicBlock *success, llvm::BasicBlock *mismatch);
// Preserve compact projection/direct-call IR when the normalized plan has no rejection tests.
bool lower_unconditional_head(ExpressionLowering &state);
// Lower the reusable matcher against caller-supplied values and selection continuations.
void lower_match_plan(ExpressionLowering &state, const semantic::MatchPlan &plan, std::span<llvm::Value *const> values,
                      llvm::BasicBlock *success, llvm::BasicBlock *mismatch);
// Match an already evaluated RHS, publishing new bindings only along the successful continuation.
llvm::Value *lower_body_match(ExpressionLowering &state, const ast::MatchExpression &match);
// Raise clause exhaustion using the existing checked generated-call contract.
void raise_function_clause(ExpressionLowering &state);
// Raise a typed Erlang error; payload ownership remains with the existing checked service.
void raise_reason(ExpressionLowering &state, abi::v1::ErrorReason reason, llvm::Value *payload = nullptr);
// Evaluate only authorized immediate service operations with success-only outputs.
llvm::Value *lower_immediate(ExpressionLowering &state, abi::v1::ImmediateOperation operation, llvm::Value *left,
                             llvm::Value *right = nullptr);
// Evaluate the existing bounded body walk using the candidate's tentative bindings.
llvm::Value *lower_body(ExpressionLowering &state, const ast::ExprId &root);
// Emit an already visited ordinary value node; lazy operands are scheduled by the iterative walker.
llvm::Value *lower_value(ExpressionLowering &state, const ast::ExprId &id);

struct GuardEdges {
    // Named candidate continuations prevent accidental interchange of acceptance and rejection blocks.
    llvm::BasicBlock *success;
    llvm::BasicBlock *rejection;
};

// Preserve comma conjunctions and semicolon alternatives using canonical-true boundaries.
void lower_guard(ExpressionLowering &state, const ast::GuardSyntax &guard, GuardEdges edges);

// Emit one resolved call after its arguments have been evaluated in source order.
llvm::Value *lower_call(ExpressionLowering &state, const ast::Expression &expression, const ast::CallExpression &call);
} // namespace erlang_aot::codegen
