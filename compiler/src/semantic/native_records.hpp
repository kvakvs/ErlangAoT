#pragma once
#include "declarations.hpp"
#include <set>
#include <string>

// Compile-time rules of OTP 29 native records (docs/native-records.md#compile-time-rules).
namespace clause::semantic {
// Every default of a native record must be a literal after constant folding.
void native_defaults(const Module &module, const RecordLayout &layout, const Reporter &out);
// Index -export_record and -import_record in source order with erl_lint's rules.
void index_record_attributes(Module &module, const Reporter &out);
// Qualified and imported forms check only duplicate names and wildcards; other rules apply at run time.
void external_fields(const Module &module, const ast::RecordExpression &record, const Reporter &out);
// A local native construction must give every field without a default a value.
void native_initialized(const Module &module, const ast::RecordExpression &record, const RecordLayout &layout,
                        const std::set<std::u32string> &named, const Reporter &out);
} // namespace clause::semantic
