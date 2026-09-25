#include "forms.hpp"
#include "parsing/delimiters.hpp"
#include <algorithm>

namespace erlang_aot {
void FormParser::work(const std::size_t amount) const {
    if (amount > work_) {
        fail(DiagnosticCode::resource_limit, "parser work budget exhausted");
    }
    work_ -= amount;
}

void FormParser::expected(std::string description) const {
    auto diagnostic = token_diagnostic(DiagnosticCode::parser_syntax, "expected " + description, cursor_.anchor());
    diagnostic.expected = std::move(description);
    throw DiagnosticError(std::move(diagnostic));
}

void FormParser::enrich(Diagnostic &diagnostic) const {
    if (diagnostic.code == DiagnosticCode::resource_limit) {
        return;
    }
    auto count = std::min(cursor_.offset(), tokens_.size());
    if (count != 0 && tokens_[count - 1].spelling.source == diagnostic.primary.source &&
        tokens_[count - 1].spelling.begin == diagnostic.primary.begin) {
        --count;
    }
    Delimiters delimiters;
    for (std::size_t i = 0; i < count; ++i) {
        (void)delimiters.boundary(tokens_[i]);
        delimiters.consume(tokens_.subspan(i));
    }
    const auto *open = delimiters.opener();
    if (!open) {
        return;
    }
    diagnostic.opener = open->location;
    diagnostic.related.push_back(open->spelling);
    diagnostic.related.insert(diagnostic.related.end(), open->origins.begin(), open->origins.end());
}
} // namespace erlang_aot
