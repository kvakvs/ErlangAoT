#include "children.hpp"

namespace erlang_aot::ast {
void Children::qualifier(const Qualifier &value) const {
    source(value.source);
    std::visit([this](const auto &item) { qualifier_value(item); }, value.value);
}

void Children::qualifier(const ZippedQualifier &value) const {
    source(value.source);
    if (value.qualifiers.size() < 2) {
        throw std::invalid_argument("zip requires at least two qualifiers");
    }
    for (const auto &item : value.qualifiers) {
        qualifier(item);
    }
}

void Children::qualifiers(const std::vector<ComprehensionQualifier> &values) const {
    if (values.empty()) {
        throw std::invalid_argument("empty comprehension qualifiers");
    }
    for (const auto &item : values) {
        std::visit([this](const auto &value) { qualifier(value); }, item);
    }
}

void Children::qualifier_value(const FilterQualifier &value) const { child(value.expression); }

void Children::qualifier_value(const ListGenerator &value) const {
    pattern(value.pattern, false);
    child(value.input);
}

void Children::qualifier_value(const BinaryGenerator &value) const {
    qualifier_value(ListGenerator{.pattern = value.pattern, .input = value.input, .strict = value.strict});
    if (const auto &[expression] = std::get<PatternCandidate>(builder.view().pattern(value.pattern).value);
        !std::holds_alternative<Bitstring>(builder.view().expression(expression).value)) {
        throw std::invalid_argument("binary generator requires binary syntax");
    }
}

void Children::qualifier_value(const MapGenerator &value) const {
    pattern(value.key, false);
    pattern(value.value, false);
    child(value.input);
}

void Children::operator()(const ListComprehension &value) const {
    body(value.templates);
    qualifiers(value.qualifiers);
}

void Children::operator()(const MapComprehension &value) const {
    if (value.templates.empty()) {
        throw std::invalid_argument("empty map comprehension templates");
    }
    for (const auto &field : value.templates) {
        source(field.source);
        child(field.key);
        child(field.value);
    }
    qualifiers(value.qualifiers);
}

void Children::operator()(const BinaryComprehension &value) const {
    child(value.expression);
    qualifiers(value.qualifiers);
}
} // namespace erlang_aot::ast
