#pragma once
#include <erlang_aot/runtime/process_context.hpp>
#include <string_view>

namespace erlang_aot::runtime::detail {
// Name the class of an exception: exit/1 and throw/1 keep theirs, every other reason is an error.
std::string_view exception_class(const CallFailure &failure);
// Name the error atom of a typed language failure (badmatch, case_clause, ...); raised reasons have none.
std::string_view error_name(abi::v1::ErrorReason reason) noexcept;
// Build the Erlang reason of a pending exception: the raised term, the error atom or {Atom, Value}.
TermResult<Term> exception_reason_term(ProcessContext &context, const CallFailure &failure);
// The stack trace term of an exception: the given stack, or the captured frames innermost first, where the top
// frame shows the erlang:error/2,3 arguments when present.
TermResult<Term> stack_term(ProcessContext &context, const CallFailure &failure);
} // namespace erlang_aot::runtime::detail
