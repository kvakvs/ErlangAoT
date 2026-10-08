#pragma once
#include <clause/runtime/process_context.hpp>
#include <clause/runtime/runtime.hpp>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace clause::runtime::detail {
// Decode an erlang:halt/1 slogan (a proper list of at most 1023 Unicode code points) to UTF-8.
std::optional<std::string> slogan_text(const Term &value);

// Runtime options of one program run and how many leading arguments carried them (docs/executables.md).
struct ProgramOptions {
    // Options for Runtime::start; defaults unless CLAUSE_FLAGS or leading arguments set them.
    RuntimeOptions runtime;
    // Leading command-line arguments after the program name used as runtime options, `--` included.
    std::size_t consumed = 0;
};

// Parse CLAUSE_FLAGS, then the leading arguments; the error is the text of an invalid or unimplemented option.
std::expected<ProgramOptions, std::string> program_options(int argc, char **argv);
// Build the entry argument: a proper list of strings, decoded from the platform's native arguments after the
// first `skip` ones (runtime options).
TermResult<Term> program_arguments(ProcessContext &context, int argc, char **argv, std::size_t skip);
// Render the uncaught-exception reason of a failed entry call in ~w form.
TermResult<std::string> exception_reason(const CallFailure &failure);
} // namespace clause::runtime::detail
