#pragma once
#include <erlang_aot/runtime/process_context.hpp>

// Why processes end (docs/processes.md#exits): their exit reasons and the error reports of crashed processes.
namespace erlang_aot::runtime::detail {
// The exit reason of an ended process, built in its heap: normal after a return, the reason of exit/1,
// {Reason, Stack} for an error and {{nocatch, Value}, Stack} for an uncaught throw.
TermResult<Term> exit_reason(ProcessContext &process);
// Write OTP's error report on stderr for a process that ended with an error or an uncaught throw; a return or an
// exit writes nothing.
void report_exit(ProcessContext &process) noexcept;
} // namespace erlang_aot::runtime::detail
