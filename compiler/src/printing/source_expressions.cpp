#include "source_printer.hpp"

namespace clause::printing {
namespace {
// A record name as written: `r`, `m:r` or the anonymous `_`.
std::string record_name(const ast::RecordIdentity &identity) {
    if (const auto *local = std::get_if<ast::UnresolvedRecordName>(&identity.value)) {
        return atom_text(local->name);
    }
    if (const auto *qualified = std::get_if<ast::QualifiedRecordName>(&identity.value)) {
        return atom_text(qualified->module) + ':' + atom_text(qualified->name);
    }
    return "_";
}

// An atom or variable part of fun M:F/A.
std::string name_part(const std::variant<ast::Atom, ast::Variable> &part) {
    const auto *atom = std::get_if<ast::Atom>(&part);
    return atom ? atom_text(*atom) : utf8(std::get<ast::Variable>(part).name);
}

// The type modifiers of a binary segment: `integer-little-unit:8`.
std::string modifiers(const std::vector<ast::BinaryModifier> &items) {
    std::string result;
    for (const auto &item : items) {
        result += (result.empty() ? "" : "-") + atom_text(item.name) +
                  (item.parameter ? ':' + item.parameter->decimal : std::string());
    }
    return result;
}

// The expressions printed on one line; their operands keep the place's annotation choice.
struct Inline {
    const SourcePrinter &printer;
    Place place;
    // The whole expression, for the families printed elsewhere.
    const ast::ExprValue &whole;

    // An operand of this expression.
    std::string operand(const ast::ExprId &id) const {
        return printer.expression(id, {place.indent, false, place.annotated});
    }

    std::string operator()(const ast::Atom &value) const { return atom_text(value); }

    std::string operator()(const ast::Variable &value) const { return utf8(value.name); }

    std::string operator()(const ast::IntegerLiteral &value) const { return value.value.decimal; }

    std::string operator()(const ast::FloatLiteral &value) const {
        return literal_text(TokenKind::floating, value.value);
    }

    std::string operator()(const ast::CharacterLiteral &value) const {
        return literal_text(TokenKind::character, Integer{std::to_string(static_cast<std::uint32_t>(value.value))});
    }

    std::string operator()(const ast::StringLiteral &value) const {
        return literal_text(TokenKind::string, value.value);
    }

    std::string operator()(const ast::Tuple &value) const { return '{' + printer.list(value.elements, place) + '}'; }

    std::string operator()(const ast::List &value) const {
        const auto tail = value.tail ? " | " + operand(*value.tail) : std::string();
        return '[' + printer.list(value.elements, place) + tail + ']';
    }

    std::string operator()(const ast::Group &value) const { return '(' + operand(value.expression) + ')'; }

    std::string operator()(const ast::Bitstring &value) const {
        std::string result;
        for (const auto &segment : value.segments) {
            auto text = operand(segment.value);
            if (segment.size) {
                text += ':' + operand(*segment.size);
            }
            if (segment.modifiers) {
                text += '/' + modifiers(*segment.modifiers);
            }
            result += (result.empty() ? "" : ", ") + text;
        }
        return "<<" + result + ">>";
    }

    std::string operator()(const ast::UnaryExpression &value) const {
        return unary_text(value.operation, operand(value.operand));
    }

    std::string operator()(const ast::BinaryExpression &value) const {
        return operand(value.left) + ' ' + operator_text(value.operation) + ' ' + operand(value.right);
    }

    std::string operator()(const ast::MatchExpression &value) const {
        // The left side is a pattern: it carries no annotation.
        return printer.expression(value.left, {place.indent, false, false}) + " = " + operand(value.right);
    }

    std::string operator()(const ast::CatchExpression &value) const { return "catch " + operand(value.expression); }

    std::string operator()(const ast::CallExpression &value) const {
        return operand(value.target) + '(' + printer.list(value.arguments, place) + ')';
    }

    std::string operator()(const ast::RemoteExpression &value) const {
        return operand(value.module) + ':' + operand(value.function);
    }

    std::string operator()(const ast::MapExpression &value) const {
        std::string fields;
        for (const auto &field : value.fields) {
            fields += (fields.empty() ? "" : ", ") + operand(field.key) +
                      (field.kind == ast::MapFieldKind::exact ? " := " : " => ") + operand(field.value);
        }
        return (value.base ? operand(*value.base) : std::string()) + "#{" + fields + '}';
    }

    std::string operator()(const ast::RecordExpression &value) const {
        std::string fields;
        for (const auto &field : value.fields) {
            const auto *atom = std::get_if<ast::Atom>(&field.name);
            const auto name = atom ? atom_text(*atom) : utf8(std::get<ast::Variable>(field.name).name);
            fields += (fields.empty() ? "" : ", ") + name + " = " + operand(field.value);
        }
        return (value.base ? operand(*value.base) : std::string()) + '#' + record_name(value.identity) + '{' + fields +
               '}';
    }

    std::string operator()(const ast::RecordAccess &value) const {
        return operand(value.base) + '#' + record_name(value.identity) + '.' + atom_text(value.field);
    }

    std::string operator()(const ast::RecordIndex &value) const {
        return '#' + atom_text(value.record) + '.' + atom_text(value.field);
    }

    std::string operator()(const ast::LocalFunReference &value) const {
        return "fun " + atom_text(value.name) + '/' + value.arity.decimal;
    }

    std::string operator()(const ast::RemoteFunReference &value) const {
        const auto *arity = std::get_if<Integer>(&value.arity);
        return "fun " + name_part(value.module) + ':' + name_part(value.name) + '/' +
               (arity ? arity->decimal : utf8(std::get<ast::Variable>(value.arity).name));
    }

    // Block expressions and comprehensions have their own printers.
    template <typename Value> std::string operator()(const Value &) const {
        return control(whole) ? control_text(printer, whole, place.indent)
                              : comprehension_text(printer, whole, place.indent);
    }
};
} // namespace

std::string SourcePrinter::expression(const ast::ExprId &id, Place place) const {
    const auto &node = syntax_.expression(id);
    auto text = std::visit(Inline{*this, place, node.value}, node.value);
    // Only an expression that is a whole line's outermost one carries its note, moved to that line's end.
    const auto note = place.annotated && place.statement && notes_.expression ? notes_.expression(node) : std::nullopt;
    return note ? text + NOTE_START + *note + NOTE_END : text;
}
} // namespace clause::printing
