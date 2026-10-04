#pragma once
#include <erlang_aot/runtime/process_context.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace erlang_aot::runtime::detail {
// Decode an erlang:halt/1 slogan (a proper list of at most 1023 Unicode code points) to UTF-8.
std::optional<std::string> slogan_text(const Term &value);
// Build the entry argument: a proper list of strings, decoded from the platform's native arguments.
TermResult<Term> program_arguments(ProcessContext &context, int argc, char **argv);
// Name the class of an uncaught exception: exit/1 and throw/1 keep theirs, every other reason is an error.
std::string_view exception_class(const CallFailure &failure);
// Render the uncaught-exception reason of a failed entry call in ~w form.
TermResult<std::string> exception_reason(const CallFailure &failure);
} // namespace erlang_aot::runtime::detail
