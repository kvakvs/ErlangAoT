#include "parsing/operator_info.hpp"
#include "tree.hpp"

namespace erlang_aot::printing {
namespace {
// Keep declaration categories readable without exposing enum ordinals.
std::string_view declaration_kind(ast::TypeDeclarationKind kind) {
    switch (kind) {
    case ast::TypeDeclarationKind::alias:
        return "type";
    case ast::TypeDeclarationKind::opaque:
        return "opaque";
    case ast::TypeDeclarationKind::nominal:
        return "nominal";
    }
    throw std::invalid_argument("unknown type declaration kind");
}
} // namespace

void TreePrinter::visit(const ast::TypeId &id) { module_.visit(id, *this); }

void TreePrinter::operator()(const ast::TypeGroup &value) {
    output_ << "TypeGroup";
    child("type", value.type);
}

void TreePrinter::operator()(const ast::AnnotatedType &value) {
    output_ << "AnnotatedType variable=" << utf8(value.variable.name);
    child("type", value.type);
}

void TreePrinter::operator()(const ast::UnionType &value) {
    output_ << "UnionType";
    child("left", value.left);
    child("right", value.right);
}

void TreePrinter::operator()(const ast::RangeType &value) {
    output_ << "RangeType";
    child("first", value.first);
    child("last", value.last);
}

void TreePrinter::operator()(const ast::UnaryType &value) {
    output_ << "UnaryType operator=" << utf8(operator_spelling(value.operation));
    child("operand", value.operand);
}

void TreePrinter::operator()(const ast::BinaryTypeOperator &value) {
    output_ << "BinaryTypeOperator operator=" << utf8(operator_spelling(value.operation));
    child("left", value.left);
    child("right", value.right);
}

void TreePrinter::operator()(const ast::TypeApplication &value) {
    output_ << "TypeApplication name=" << atom(value.name) << " predefined=" << value.predefined;
    if (value.module)
        output_ << " module=" << atom(*value.module);
    handles("argument", value.arguments);
}

void TreePrinter::operator()(const ast::TupleType &value) {
    output_ << "TupleType any=" << value.any;
    handles("element", value.elements);
}

void TreePrinter::operator()(const ast::ListType &value) {
    output_ << "ListType nonempty=" << value.nonempty;
    if (value.element)
        child("element", *value.element);
}

void TreePrinter::operator()(const ast::MapType &value) {
    output_ << "MapType any=" << value.any;
    objects("field", value.fields);
}

void TreePrinter::operator()(const ast::MapTypeField &value) {
    output_ << "MapTypeField kind=" << (value.kind == ast::MapFieldKind::associate ? "=>" : ":=");
    child("key", value.key);
    child("value", value.value);
}

void TreePrinter::operator()(const ast::RecordType &value) {
    output_ << "RecordType name=" << atom(value.name);
    if (value.module)
        output_ << " module=" << atom(*value.module);
    objects("field", value.fields);
}

void TreePrinter::operator()(const ast::RecordTypeField &value) {
    output_ << "RecordTypeField name=" << atom(value.name);
    child("type", value.type);
}

void TreePrinter::operator()(const ast::BitstringType &value) {
    output_ << "BitstringType";
    if (value.base)
        child("base", *value.base);
    if (value.unit)
        child("unit", *value.unit);
}

void TreePrinter::operator()(const ast::FunType &value) {
    output_ << "FunType arguments=" << (value.arguments ? std::to_string(value.arguments->size()) : "any");
    if (value.arguments)
        handles("argument", *value.arguments);
    if (value.result)
        child("result", *value.result);
}

void TreePrinter::operator()(const ast::TypeDeclaration &value) {
    output_ << "TypeDeclaration kind=" << declaration_kind(value.kind) << " name=" << atom(value.name);
    for (const auto &variable : value.parameters)
        output_ << " parameter=" << utf8(variable.name);
    child("type", value.type);
}
} // namespace erlang_aot::printing
