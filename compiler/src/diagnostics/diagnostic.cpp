#include <erlang_aot/compiler/diagnostic.hpp>
#include <utility>

namespace erlang_aot {
LexicalError::LexicalError(Diagnostic value)
    : std::runtime_error(value.message), diagnostic(std::move(value)) {}

std::string render(const Diagnostic& diagnostic)
{
    const auto& span = diagnostic.primary;
    const auto position = span.source->position(span.begin);
    return span.source->name + ':' + std::to_string(position.line) + ':'
        + std::to_string(position.column) + ": " + diagnostic.message;
}
} // namespace erlang_aot
