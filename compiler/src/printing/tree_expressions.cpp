#include "parsing/operator_info.hpp"
#include "printable.hpp"
#include "tree.hpp"

namespace erlang_aot::printing {
std::optional<char32_t> TreePrinter::string_character(const ast::ExprId &id) const {
    const auto *integer = std::get_if<ast::IntegerLiteral>(&module_.expression(id).value);
    return integer ? printable_character(integer->value) : std::nullopt;
}

void TreePrinter::operator()(const ast::Atom &value) const { output_ << "Atom name=" << atom(value); }

void TreePrinter::operator()(const ast::Variable &value) const { output_ << "Variable name=" << utf8(value.name); }

void TreePrinter::operator()(const ast::IntegerLiteral &value) const {
    output_ << "IntegerLiteral value=" << value.value.decimal;
}

void TreePrinter::operator()(const ast::FloatLiteral &value) const {
    output_ << "FloatLiteral value=" << literal(TokenKind::floating, value.value);
}

void TreePrinter::operator()(const ast::CharacterLiteral &value) const {
    output_ << "CharacterLiteral value=" << literal(TokenKind::character, Integer{std::to_string(value.value)});
}

void TreePrinter::operator()(const ast::StringLiteral &value) const {
    output_ << "StringLiteral value=" << literal(TokenKind::string, value.value);
}

void TreePrinter::operator()(const ast::Tuple &value) {
    output_ << "Tuple elements=" << value.elements.size();
    handles("element", value.elements);
}

void TreePrinter::operator()(const ast::List &value) {
    if (!value.tail && string_list(value.elements)) {
        return;
    }
    output_ << "[count=" << value.elements.size() << " tail=" << (value.tail ? "explicit" : "nil");
    handles("element", value.elements);
    optional_child("tail", value.tail);
}

void TreePrinter::operator()(const ast::Group &value) {
    output_ << "Group";
    child("expression", value.expression);
}

void TreePrinter::operator()(const ast::UnaryExpression &value) {
    output_ << "UnaryExpression operator=" << utf8(operator_spelling(value.operation));
    child("operand", value.operand);
}

void TreePrinter::operator()(const ast::BinaryExpression &value) {
    output_ << "BinaryExpression operator=" << utf8(operator_spelling(value.operation));
    child("left", value.left);
    child("right", value.right);
}

void TreePrinter::operator()(const ast::MatchExpression &value) {
    output_ << "MatchExpression";
    child("left", value.left);
    child("right", value.right);
}

void TreePrinter::operator()(const ast::CatchExpression &value) {
    output_ << "CatchExpression";
    child("expression", value.expression);
}

void TreePrinter::operator()(const ast::CallExpression &value) {
    output_ << "CallExpression arguments=" << value.arguments.size();
    child("target", value.target);
    handles("argument", value.arguments);
}

void TreePrinter::operator()(const ast::RemoteExpression &value) {
    output_ << "RemoteExpression";
    child("module", value.module);
    child("function", value.function);
}
} // namespace erlang_aot::printing
