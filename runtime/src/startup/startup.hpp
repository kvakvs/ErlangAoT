#pragma once
#include <erlang_aot/runtime/process_context.hpp>
#include <optional>
#include <string>

namespace erlang_aot::runtime::detail {
// Decode an erlang:halt/1 slogan (a proper list of at most 1023 Unicode code points) to UTF-8.
std::optional<std::string> slogan_text(const Term &value);
// Build the entry argument: a proper list of strings, decoded from the platform's native arguments.
TermResult<Term> program_arguments(ProcessContext &context, int argc, char **argv);
// Render the uncaught-exception reason of a failed entry call in ~w form.
TermResult<std::string> exception_reason(const CallFailure &failure);
} // namespace erlang_aot::runtime::detail
