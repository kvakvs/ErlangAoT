#include "tree.hpp"

namespace erlang_aot::printing {
namespace {
struct RecordName {
    // Keep identity kind and decoded names together on their owning node's line.
    std::string operator()(const ast::UnresolvedRecordName &value) const { return "local name=" + atom(value.name); }

    std::string operator()(const ast::QualifiedRecordName &value) const {
        return "qualified module=" + atom(value.module) + " name=" + atom(value.name);
    }

    std::string operator()(const ast::InferredRecordName &) const { return "inferred"; }
};

struct FieldName {
    // Preserve atom/variable field-name syntax, including the wildcard variable.
    std::string operator()(const ast::Atom &value) const { return "Atom name=" + atom(value); }

    std::string operator()(const ast::Variable &value) const { return "Variable name=" + utf8(value.name); }
};
} // namespace

void TreePrinter::operator()(const ast::MapExpression &value) {
    output_ << "MapExpression mode=" << (value.base ? "update" : "construct") << " fields=" << value.fields.size();
    optional_child("base", value.base);
    objects("field", value.fields);
}

void TreePrinter::operator()(const ast::MapField &value) {
    output_ << "MapField operator=" << (value.kind == ast::MapFieldKind::associate ? "=>" : ":=");
    child("key", value.key);
    child("value", value.value);
}

void TreePrinter::operator()(const ast::RecordExpression &value) {
    output_ << "RecordExpression mode=" << (value.base ? "update" : "construct")
            << " identity=" << std::visit(RecordName{}, value.identity.value) << " fields=" << value.fields.size();
    optional_child("base", value.base);
    objects("field", value.fields);
}

void TreePrinter::operator()(const ast::RecordAccess &value) {
    output_ << "RecordAccess identity=" << std::visit(RecordName{}, value.identity.value)
            << " field=" << atom(value.field);
    child("base", value.base);
}

void TreePrinter::operator()(const ast::RecordIndex &value) {
    output_ << "RecordIndex record=" << atom(value.record) << " field=" << atom(value.field);
}

void TreePrinter::operator()(const ast::RecordField &value) {
    output_ << "RecordField " << std::visit(FieldName{}, value.name);
    child("value", value.value);
}

void TreePrinter::operator()(const ast::Bitstring &value) {
    output_ << "Bitstring segments=" << value.segments.size();
    objects("segment", value.segments);
}

void TreePrinter::operator()(const ast::BinarySegment &value) {
    output_ << "BinarySegment size=" << (value.size ? "explicit" : "omitted")
            << " modifiers=" << (value.modifiers ? std::to_string(value.modifiers->size()) : "omitted");
    child("value", value.value);
    optional_child("size", value.size);
    if (value.modifiers) {
        objects("modifier", *value.modifiers);
    }
}

void TreePrinter::operator()(const ast::BinaryModifier &value) {
    output_ << "BinaryModifier name=" << atom(value.name)
            << " parameter=" << (value.parameter ? value.parameter->decimal : "none");
}
} // namespace erlang_aot::printing
