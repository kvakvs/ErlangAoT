#include "printable.hpp"
#include "tree.hpp"

namespace erlang_aot::printing {
std::optional<char32_t> TreePrinter::string_character(const ast::TermId &id) const {
    const auto *integer = std::get_if<ast::IntegerLiteral>(&module_.term(id).value);
    return integer ? printable_character(integer->value) : std::nullopt;
}

void TreePrinter::visit(const ast::TermId &id) { module_.visit(id, *this); }

void TreePrinter::arities(const std::vector<ast::NameArity> &values) const {
    for (const auto &[name, arity] : values) {
        output_ << ' ' << atom(name) << '/' << arity.decimal;
    }
}

void TreePrinter::operator()(const ast::ExportAttribute &value) const {
    output_ << "ExportAttribute";
    arities(value.functions);
}

void TreePrinter::operator()(const ast::ImportAttribute &value) const {
    output_ << "ImportAttribute module=" << atom(value.module);
    arities(value.functions);
}

void TreePrinter::operator()(const ast::ImportRecordAttribute &value) const {
    output_ << "ImportRecordAttribute module=" << atom(value.module);
    for (const auto &name : value.names) {
        output_ << ' ' << atom(name);
    }
}

void TreePrinter::operator()(const ast::GenericAttribute &value) {
    output_ << "GenericAttribute name=" << atom(value.name);
    child("value", value.value);
}

void TreePrinter::operator()(const ast::RecordDeclaration &value) {
    output_ << "RecordDeclaration name=" << atom(value.name) << " native=" << value.native;
    objects("field", value.fields);
}

void TreePrinter::operator()(const ast::RecordDeclarationField &value) {
    output_ << "RecordDeclarationField name=" << atom(value.name);
    optional_child("default", value.default_value);
    if (value.type) {
        child("type", *value.type);
    }
}

void TreePrinter::operator()(const ast::DocumentationAttribute &value) {
    output_ << "DocumentationAttribute module=" << value.module;
    if (const auto *literal = std::get_if<ast::TermId>(&value.value)) {
        child("value", *literal);
    } else {
        objects("metadata", std::get<std::vector<ast::DocumentationEntry>>(value.value));
    }
}

void TreePrinter::operator()(const ast::DocumentationEntry &value) {
    output_ << "DocumentationEntry";
    child("key", value.key);
    std::visit([this](const auto &id) { child("value", id); }, value.value);
}

void TreePrinter::operator()(const ast::TermTuple &value) {
    output_ << "TermTuple elements=" << value.elements.size();
    handles("element", value.elements);
}

void TreePrinter::operator()(const ast::TermList &value) {
    if (!value.tail && string_list(value.elements)) {
        return;
    }
    output_ << "TermList elements=" << value.elements.size();
    handles("element", value.elements);
    if (value.tail) {
        child("tail", *value.tail);
    }
}

void TreePrinter::operator()(const ast::TermMap &value) {
    output_ << "TermMap entries=" << value.entries.size();
    for (const auto &[key, mapped] : value.entries) {
        child("key", key);
        child("value", mapped);
    }
}

void TreePrinter::operator()(const ast::TermBits &value) const {
    output_ << "TermBits bits=";
    for (const auto bit : value.bits) {
        output_ << (bit ? '1' : '0');
    }
}

void TreePrinter::operator()(const ast::TermFunction &value) const {
    output_ << "TermFunction target=" << atom(value.module) << ':' << atom(value.name) << '/' << value.arity.decimal;
}
} // namespace erlang_aot::printing
