#pragma once
#include <erlang_aot/runtime/process_context.hpp>
#include <string_view>

namespace erlang_aot::runtime::detail {
// Name the error atom of a typed language failure (badmatch, case_clause, ...); raised reasons have none.
std::string_view error_name(abi::v1::ErrorReason reason) noexcept;
// Build the Erlang reason of a pending exception: the raised term, the error atom or {Atom, Value}.
TermResult<Term> exception_reason_term(ProcessContext &context, const CallFailure &failure);
} // namespace erlang_aot::runtime::detail
