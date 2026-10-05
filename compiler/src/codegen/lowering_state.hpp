#pragma once
#include "../semantic/match_plan.hpp"
#include "lowering_expressions.hpp"
#include "lowering_roots.hpp"
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/abi/containers.hpp>
#include <map>
#include <string_view>

namespace erlang_aot::codegen {
using BindingReads = std::map<const ast::Expression *, semantic::BindingId>;

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
    // Borrow the function-wide read index; SSA values still belong to each isolated candidate.
    const BindingReads *reads = nullptr;
    // Select the source candidate without allowing bindings from another clause to enter its environment.
    std::size_t clause = 0;
    // Share argument roots and bounded temporary slots across all candidates of the generated function.
    FunctionRoots *roots = nullptr;
    // Match definitions retain the original candidate SSA word; repeated names read this same identity.
    std::map<semantic::BindingId, llvm::Value *> bindings = {};
    // Share a terminal failure exit across calls instead of duplicating return blocks per expression.
    llvm::BasicBlock *failure = nullptr;
    // The innermost enclosing `catch` handler; failures and raises branch there instead of leaving the function.
    llvm::BasicBlock *handler = nullptr;
    // Semantic service errors reject the enclosing guard, while body errors raise badarg.
    llvm::BasicBlock *rejection = nullptr;
    llvm::BasicBlock *bad_argument = nullptr;
    // Arithmetic operand failures share a body error exit while guards retain their rejection edge.
    llvm::BasicBlock *bad_arithmetic = nullptr;
    // Capture each evaluated record field immediately, including repeated wildcard/default source nodes.
    std::map<const ast::Expression *, std::vector<llvm::Value *>> record_values = {};
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
// Plan a one-input body or case pattern; semantic analysis accepted it, so failure is a phase-contract error.
semantic::MatchPlan body_pattern_plan(const ExpressionLowering &state, const ast::ExprId &pattern);
// Match an already evaluated RHS, publishing new bindings only along the successful continuation.
llvm::Value *lower_body_match(ExpressionLowering &state, const ast::MatchExpression &match);
// Raise clause exhaustion using the existing checked generated-call contract.
void raise_function_clause(ExpressionLowering &state);
// Raise a typed Erlang error; payload ownership remains with the existing checked service.
void raise_reason(ExpressionLowering &state, abi::v1::ErrorReason reason, llvm::Value *payload = nullptr);
// Evaluate only authorized immediate service operations with success-only outputs.
llvm::Value *lower_immediate(ExpressionLowering &state, abi::v1::ImmediateOperation operation, llvm::Value *left,
                             llvm::Value *right = nullptr);
// Print one term through erlang:display/1; the rooted result is the atom true after the channel check.
llvm::Value *lower_display(ExpressionLowering &state, llvm::Value *value);
// Stop the program through erlang:halt/0,1 (null `status` means halt/0); the dead continuation yields [].
llvm::Value *lower_halt(ExpressionLowering &state, llvm::Value *status);
// Raise `reason` with the class of erlang:error/exit/throw (`name`); the dead continuation yields [].
llvm::Value *lower_raise(ExpressionLowering &state, std::u32string_view name, llvm::Value *reason);
// Turn the pending exception into the `catch Expr` value; halts and runtime failures continue to the outer exit.
llvm::Value *lower_catch(ExpressionLowering &state);
// Ordinary service errors reject guards or raise badarg; boolean operand errors additionally retain their value.
llvm::BasicBlock *bad_argument_exit(ExpressionLowering &state, llvm::Value *payload = nullptr);
// Arithmetic errors reject guards and raise badarith in ordinary bodies.
llvm::BasicBlock *bad_arithmetic_exit(ExpressionLowering &state);
// Materialize arbitrary decimal literals with target-specific small encodings or rooted runtime storage.
// Materialize finite IEEE bits through a rooted checked runtime service.
llvm::Value *lower_float(ExpressionLowering &state, double value);
llvm::Value *lower_integer(ExpressionLowering &state, std::string_view decimal);
// Use checked small arithmetic where safe, retaining the common runtime fallback for all other values.
llvm::Value *lower_operation(ExpressionLowering &state, abi::v1::ImmediateOperation operation, llvm::Value *left,
                             llvm::Value *right = nullptr);

struct ServiceOutput {
    // Pair the status byte with its success-only rooted word slot, without interchangeable positional pointers.
    llvm::Value *outcome;
    llvm::Value *slot;
};

// Consume a checked success-only output after separating infrastructure failure from semantic rejection.
llvm::Value *checked_value(ExpressionLowering &state, ServiceOutput result, llvm::BasicBlock *rejection);

struct BitLowering {
    // Success-only words retain both extracted ownership and the following logical bit cursor.
    llvm::Value *value;
    llvm::Value *cursor;
};

// Construct complete bitstrings and checked byte parts through shared rooted services.
llvm::Value *lower_bits(ExpressionLowering &state, const ast::Bitstring &binary);
llvm::Value *lower_binary_part(ExpressionLowering &state, std::span<llvm::Value *const> values);
// Reject malformed/truncated segments without publishing tentative bindings or advancing their cursor.
BitLowering lower_bit_pattern(ExpressionLowering &state, const semantic::MatchNode &node,
                              std::span<llvm::Value *> values, llvm::BasicBlock *mismatch);
// Stage source-ordered map construction/update through rooted checked services.
llvm::Value *lower_map(ExpressionLowering &state, const ast::MapExpression &map);
// Resolve map BIFs separately from the two-operand numeric service; return null for other operations.
llvm::Value *lower_map_query(ExpressionLowering &state, abi::v1::ImmediateOperation operation, llvm::Value *left,
                             llvm::Value *right);
// Evaluate scoped keys with semantic errors routed to the enclosing pattern mismatch.
llvm::Value *lower_map_pattern(ExpressionLowering &state, const semantic::MatchNode &node, llvm::Value *input,
                               llvm::BasicBlock *mismatch);
// Construct tuple/list/string expression values after their source-ordered children have completed.
llvm::Value *lower_container(ExpressionLowering &state, const ast::ExprValue &value);
// Share rooted tuple construction with ordinary record expansion.
llvm::Value *lower_tuple(ExpressionLowering &state, std::span<llvm::Value *const> values);
// Lower record values/access/indices using tuple shape and checked element services.
llvm::Value *lower_record(ExpressionLowering &state, const ast::ExprId &id);
// Compose the tuple-record BIF with context-appropriate argument rejection and literal declaration sizes.
llvm::Value *lower_record_test(ExpressionLowering &state, const ast::Expression &expression,
                               const ast::CallExpression &call);
// Validate both arbitrary-integer bounds first, then test the candidate on a separate successful edge.
llvm::Value *lower_integer_range(ExpressionLowering &state, const ast::CallExpression &call);
// Check candidate ownership/shape before extracting a rooted child; mismatch belongs to the pattern caller.
llvm::Value *lower_inspection(ExpressionLowering &state, abi::v1::ContainerInspection operation, llvm::Value *value,
                              std::size_t index, llvm::BasicBlock *mismatch);
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
