#pragma once
#include "declarations.hpp"
#include <set>
#include <string>

// Compile-time rules of OTP 29 native records (docs/native-records.md#compile-time-rules).
namespace erlang_aot::semantic {
// Every default of a native record must be a literal after constant folding.
void native_defaults(const Module &module, const RecordLayout &layout, const Reporter &out);
// A local native construction must give every field without a default a value.
void native_initialized(const Module &module, const ast::RecordExpression &record, const RecordLayout &layout,
                        const std::set<std::u32string> &named, const Reporter &out);
} // namespace erlang_aot::semantic
