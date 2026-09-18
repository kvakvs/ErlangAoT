#include "builder.hpp"

namespace erlang_aot::ast {
namespace {
// Enumerate every child-bearing payload; scalar overloads explicitly have no children.
struct Children {
    const Builder &builder;
    const OriginId &form;

    void child(const ExprId &id) const {
        if (builder.view().expression(id).source.form != form) {
            throw std::invalid_argument("AST child belongs to another form");
        }
    }

    void operator()(const Atom &) const {}

    void operator()(const Variable &) const {}

    void operator()(const IntegerLiteral &) const {}

    void operator()(const FloatLiteral &) const {}

    void operator()(const CharacterLiteral &) const {}

    void operator()(const StringLiteral &) const {}

    void operator()(const BinarySigilLiteral &) const {}

    void operator()(const UnaryExpression &value) const { child(value.operand); }

    void operator()(const BinaryExpression &value) const {
        child(value.left);
        child(value.right);
    }

    void operator()(const MatchExpression &value) const {
        child(value.left);
        child(value.right);
    }

    void operator()(const CatchExpression &value) const { child(value.expression); }

    void operator()(const RemoteExpression &value) const {
        child(value.module);
        child(value.function);
    }

    void operator()(const CallExpression &value) const {
        child(value.target);
        for (const auto &id : value.arguments)
            child(id);
    }

    void operator()(const Group &value) const { child(value.expression); }

    void operator()(const Tuple &value) const {
        for (const auto &id : value.elements) {
            child(id);
        }
    }

    void operator()(const List &value) const {
        if (value.tail && value.elements.empty()) {
            throw std::invalid_argument("list tail requires a head");
        }
        for (const auto &id : value.elements) {
            child(id);
        }
        if (value.tail) {
            child(*value.tail);
        }
    }
};
} // namespace

void Builder::validate(const ExprValue &value) const {
    if (!active_) {
        throw std::logic_error("AST children require a form transaction");
    }
    std::visit(Children{*this, *active_}, value);
}
} // namespace erlang_aot::ast
