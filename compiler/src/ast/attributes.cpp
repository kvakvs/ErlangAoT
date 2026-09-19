#include "children.hpp"
#include "storage.hpp"

namespace erlang_aot::ast {
TermId Builder::term(TermValue value, NodeSource source) {
    if (!active_)
        throw std::logic_error("literal term requires active transaction");
    validate(source);
    std::visit(Children{*this, *active_}, value);
    return module_.storage_->terms.append({std::move(value), std::move(source)});
}

void Children::operator()(const TermTuple &value) const {
    for (const auto &id : value.elements)
        child(id);
}

void Children::operator()(const TermList &value) const {
    for (const auto &id : value.elements)
        child(id);
    if (value.tail)
        child(*value.tail);
}

void Children::operator()(const TermMap &value) const {
    for (const auto &[key, mapped] : value.entries) {
        child(key);
        child(mapped);
    }
}

void Children::operator()(const RecordDeclaration &value) const {
    for (const auto &field : value.fields) {
        source(field.source);
        if (field.default_value)
            child(*field.default_value);
    }
}

void Children::operator()(const DocumentationAttribute &value) const {
    if (const auto *literal = std::get_if<TermId>(&value.value)) {
        child(*literal);
        return;
    }
    for (const auto &entry : std::get<std::vector<DocumentationEntry>>(value.value)) {
        child(entry.key);
        std::visit([this](const auto &id) { child(id); }, entry.value);
    }
}
} // namespace erlang_aot::ast
