#pragma once
#include "binding_state.hpp"

namespace erlang_aot::semantic {
// Keep the semantic catalog available to metadata validation without consulting runtime registration.
bool guard_signature(const FunctionKey &key);
// Classify supported immediate operators without widening guard legality or numeric representation support.
std::optional<abi::v1::ImmediateOperation> immediate_operator(ast::BinaryOperator operation);
// Map every authorized unary operator to the shared checked numeric/boolean service boundary.
std::optional<abi::v1::ImmediateOperation> immediate_unary(ast::UnaryOperator operation);
// Map only resolved erlang name/arity identities to executable service operations.
std::optional<abi::v1::ImmediateOperation> immediate_service(const FunctionKey &key);
// Identify explicit erlang body-only builtins (display/1, halt/0,1); they are never guard-legal.
std::optional<FunctionKey> body_builtin(const ast::Module &syntax, const ast::CallExpression &call);
// Resolve legal guard calls using the same imports/suppression rules as embedded pattern expressions.
std::optional<FunctionKey> guard_identity(BindingAnalysis &state, const ast::ExprId &id,
                                          const ast::CallExpression &call, bool top_test);
// Validate all guard operands, including unreachable ones, and record ordinary immediate service calls.
void resolve_services(Module &module, const Reporter &out, std::size_t work_limit = 1'000'000);
// Admit only inert no_auto_import compile options and explicit erlang guard-signature imports.
bool service_metadata(const ast::Module &syntax, const ast::FormValue &value);
} // namespace erlang_aot::semantic
