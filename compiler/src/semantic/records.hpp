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
// An unqualified record_info/2 call is the compile-time pseudo-function, never a local or builtin call.
bool record_info_call(const ast::Module &syntax, const ast::ExprValue &value);

struct RecordInfo {
    // The visible tuple record whose declaration answers the query.
    const RecordLayout &layout;
    // record_info(fields, R) lists the field names; record_info(size, R) is the tuple arity.
    bool fields;
};

// Resolve a valid record_info/2 call; invalid calls are diagnosed by validate_record.
std::optional<RecordInfo> record_info(const Module &module, const ast::Expression &expression);
// Enforce record test literal rules separately from ordinary body BIF argument errors.
void validate_record_test(const Module &module, const ast::Expression &expression, const ast::CallExpression &call,
                          bool guard, const Reporter &out);
} // namespace erlang_aot::semantic
