#include "children.hpp"
#include "storage.hpp"

namespace erlang_aot::ast {
TypeId Builder::type(TypeValue value, NodeSource source) const {
    if (!active_) {
        throw std::logic_error("type requires active transaction");
    }
    validate(source);
    std::visit(Children{.builder = *this, .form = *active_}, value);
    return module_.storage_->types.append({.value = std::move(value), .source = std::move(source)});
}

void Children::operator()(const TypeGroup &value) const { child(value.type); }

void Children::operator()(const AnnotatedType &value) const { child(value.type); }

void Children::operator()(const UnionType &value) const {
    child(value.left);
    child(value.right);
}

void Children::operator()(const RangeType &value) const {
    child(value.first);
    child(value.last);
}

void Children::operator()(const UnaryType &value) const { child(value.operand); }

void Children::operator()(const BinaryTypeOperator &value) const {
    child(value.left);
    child(value.right);
}

void Children::operator()(const TypeApplication &value) const {
    for (const auto &id : value.arguments) {
        child(id);
    }
}

void Children::operator()(const TupleType &value) const {
    if (value.any && !value.elements.empty()) {
        throw std::invalid_argument("unrestricted tuple type has elements");
    }
    for (const auto &id : value.elements) {
        child(id);
    }
}

void Children::operator()(const ListType &value) const {
    if (value.nonempty && !value.element) {
        throw std::invalid_argument("nonempty list type requires element");
    }
    if (value.element) {
        child(*value.element);
    }
}

void Children::operator()(const MapType &value) const {
    if (value.any && !value.fields.empty()) {
        throw std::invalid_argument("unrestricted map type has fields");
    }
    for (const auto &field : value.fields) {
        source(field.source);
        child(field.key);
        child(field.value);
    }
}

void Children::operator()(const RecordType &value) const {
    for (const auto &field : value.fields) {
        source(field.source);
        child(field.type);
    }
}

void Children::operator()(const BitstringType &value) const {
    if (value.base) {
        child(*value.base);
    }
    if (value.unit) {
        child(*value.unit);
    }
}

void Children::operator()(const FunType &value) const {
    if (value.arguments && !value.result) {
        throw std::invalid_argument("function type product requires result");
    }
    if (value.arguments) {
        for (const auto &id : *value.arguments) {
            child(id);
        }
    }
    if (value.result) {
        child(*value.result);
    }
}

void Children::operator()(const TypeDeclaration &value) const { child(value.type); }
} // namespace erlang_aot::ast
