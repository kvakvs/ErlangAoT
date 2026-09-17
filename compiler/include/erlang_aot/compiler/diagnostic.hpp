#pragma once
#include <erlang_aot/compiler/source.hpp>
#include <stdexcept>
#include <vector>

namespace erlang_aot {
struct Span {
    // Share source ownership; offsets index decoded characters, not bytes.
    SourcePtr source;
    std::size_t begin = 0;
    std::size_t end = 0;
};
enum class DiagnosticCode : std::uint8_t {
    invalid_character, invalid_number, invalid_escape, unterminated_literal,
    string_indentation, adjacent_strings, malformed_directive, missing_terminator,
    misplaced_directive, resource_limit
};
struct Diagnostic {
    // Keep stable machine-readable identity separately from presentation.
    DiagnosticCode code;
    std::string message;
    // Retain the primary spelling and related expansion/include locations.
    Span primary;
    std::vector<Span> related;
};
class LexicalError : public std::runtime_error {
public:
    // Transport a source diagnostic from a lexical helper to the form driver.
    explicit LexicalError(Diagnostic diagnostic);
    const Diagnostic diagnostic;
};
// Render a diagnostic with its physical filename and character coordinates.
std::string render(const Diagnostic& diagnostic);
} // namespace erlang_aot
