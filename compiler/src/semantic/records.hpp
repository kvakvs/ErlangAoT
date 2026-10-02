#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
// Index included declarations and validate closed defaults before function binding analysis.
void index_records(Module &module, const Reporter &out);
// Charge expanded fields at every source use, including omitted undefined/default fields.
bool record_budget(struct BindingAnalysis &state, const ast::ExprId &id);
// Resolve only visible ordinary record names; native/qualified/inferred forms keep separate capability gates.
const RecordLayout *record_layout(const Module &module, const ast::RecordIdentity &identity);
const RecordLayout *record_layout(const Module &module, const ast::Atom &name, const ast::NodeSource &source);
// Field positions are zero-based after the tuple tag, independently of initializer source order.
std::optional<std::size_t> record_field(const RecordLayout &layout, const ast::Atom &name);
// Select explicit/wildcard/default fields in declaration order; omitted pattern fields have no constraint.
std::vector<std::optional<ast::ExprId>> record_values(const Module &module, const ast::RecordExpression &record,
                                                      bool pattern);
// Diagnose missing declarations, duplicate/unknown fields and invalid wildcard initializers at original sites.
void validate_record(const Module &module, const ast::Expression &expression, const Reporter &out);
// Enforce record test literal rules separately from ordinary body BIF argument errors.
void validate_record_test(const Module &module, const ast::Expression &expression, const ast::CallExpression &call,
                          bool guard, const Reporter &out);
} // namespace erlang_aot::semantic
