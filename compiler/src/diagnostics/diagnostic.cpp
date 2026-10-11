#include <clause/compiler/diagnostic.hpp>
#include <utility>

namespace clause {
DiagnosticError::DiagnosticError(Diagnostic value) : std::runtime_error(value.message), diagnostic(std::move(value)) {}

LexicalError::LexicalError(Diagnostic value) : std::runtime_error(value.message), diagnostic(std::move(value)) {}

std::string render(const Diagnostic &diagnostic) {
    const auto &span = diagnostic.primary;
    if (!span.source) {
        return diagnostic.message;
    }
    const auto position = span.source->position(span.begin);
    const auto location =
        diagnostic.location.value_or(LogicalLocation{span.source->name, position.line, position.column});
    // Column 0 means a line-only location, as abstract format annotations without columns give; line 0 a file.
    const auto column = location.column == 0 ? std::string() : ':' + std::to_string(location.column);
    const auto line = location.line == 0 ? std::string() : ':' + std::to_string(location.line) + column;
    auto result = location.file + line + ": " + diagnostic.message;
    if (diagnostic.opener) {
        const auto &open = *diagnostic.opener;
        result += "\n  construct opened at " + open.file + ':' + std::to_string(open.line) + ':' +
                  std::to_string(open.column);
    }
    for (const auto &origin : diagnostic.related) {
        if (!origin.source) {
            continue;
        }
        const auto point = origin.source->position(origin.begin);
        result +=
            "\n  from " + origin.source->name + ':' + std::to_string(point.line) + ':' + std::to_string(point.column);
    }
    return result;
}
} // namespace clause
