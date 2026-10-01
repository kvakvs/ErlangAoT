#pragma once
#include "binding_state.hpp"

namespace erlang_aot::semantic {
// Normalize one scalar arithmetic tree with iterative traversal and the shared semantic work budget.
std::optional<PatternLiteral> pattern_constant(BindingAnalysis &state, const ast::ExprId &root);
// Validate a read-only map key or segment-size node and return its executable children.
std::vector<ast::ExprId> pattern_expression(BindingAnalysis &state, const ast::ExprId &id);
// Resolve embedded key/size calls against OTP's guard catalog, including imports and suppression metadata.
bool pattern_call(BindingAnalysis &state, const ast::ExprId &id, const ast::CallExpression &call);
// Reject context-invalid binary modifiers/value forms before any future matching operation is emitted.
void pattern_binary(BindingAnalysis &state, const ast::ExprId &id, const ast::Bitstring &binary, bool pattern = true);
// Emit a located semantic error and invalidate the enclosing analysis transaction.
void pattern_error(BindingAnalysis &state, const ast::ExprId &id, std::string message = "illegal pattern");
} // namespace erlang_aot::semantic
