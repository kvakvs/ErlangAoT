#include "printable.hpp"
#include "source_printer.hpp"

namespace erlang_aot::printing {
namespace {
// A record type name: `#r` or `#m:r`.
std::string record_type_name(const ast::RecordType &value) {
    return '#' + (value.module ? atom_text(*value.module) + ':' : std::string()) + atom_text(value.name);
}

// Type syntax as written.
struct Type {
    const SourcePrinter &printer;

    std::string operator()(const ast::Atom &value) const { return atom_text(value); }

    std::string operator()(const ast::Variable &value) const { return utf8(value.name); }

    std::string operator()(const ast::IntegerLiteral &value) const { return value.value.decimal; }

    std::string operator()(const ast::CharacterLiteral &value) const {
        return literal_text(TokenKind::character, Integer{std::to_string(static_cast<std::uint32_t>(value.value))});
    }

    std::string operator()(const ast::TypeGroup &value) const { return '(' + printer.type(value.type) + ')'; }

    std::string operator()(const ast::AnnotatedType &value) const {
        return utf8(value.variable.name) + " :: " + printer.type(value.type);
    }

    std::string operator()(const ast::UnionType &value) const {
        return printer.type(value.left) + " | " + printer.type(value.right);
    }

    std::string operator()(const ast::RangeType &value) const {
        return printer.type(value.first) + ".." + printer.type(value.last);
    }

    std::string operator()(const ast::UnaryType &value) const {
        return unary_text(value.operation, printer.type(value.operand));
    }

    std::string operator()(const ast::BinaryTypeOperator &value) const {
        return printer.type(value.left) + ' ' + operator_text(value.operation) + ' ' + printer.type(value.right);
    }

    std::string operator()(const ast::TypeApplication &value) const {
        return (value.module ? atom_text(*value.module) + ':' : std::string()) + atom_text(value.name) + '(' +
               printer.types(value.arguments) + ')';
    }

    std::string operator()(const ast::TupleType &value) const {
        return value.any ? "tuple()" : '{' + printer.types(value.elements) + '}';
    }

    std::string operator()(const ast::ListType &value) const {
        if (!value.element) {
            return "[]";
        }
        return '[' + printer.type(*value.element) + (value.nonempty ? ", ...]" : "]");
    }

    std::string operator()(const ast::MapType &value) const {
        if (value.any) {
            return "map()";
        }
        std::string fields;
        for (const auto &field : value.fields) {
            fields += (fields.empty() ? "" : ", ") + printer.type(field.key) +
                      (field.kind == ast::MapFieldKind::exact ? " := " : " => ") + printer.type(field.value);
        }
        return "#{" + fields + '}';
    }

    std::string operator()(const ast::RecordType &value) const {
        std::string fields;
        for (const auto &field : value.fields) {
            fields += (fields.empty() ? "" : ", ") + atom_text(field.name) + " :: " + printer.type(field.type);
        }
        return record_type_name(value) + '{' + fields + '}';
    }

    std::string operator()(const ast::BitstringType &value) const {
        std::string parts;
        if (value.base) {
            parts = "_:" + printer.type(*value.base);
        }
        if (value.unit) {
            parts += std::string(parts.empty() ? "" : ", ") + "_:_*" + printer.type(*value.unit);
        }
        return "<<" + parts + ">>";
    }

    std::string operator()(const ast::FunType &value) const {
        if (!value.result) {
            return "fun()";
        }
        const auto arguments = value.arguments ? printer.types(*value.arguments) : std::string("...");
        return "fun((" + arguments + ") -> " + printer.type(*value.result) + ')';
    }
};

// A list of printable characters reads better as a string.
std::optional<std::u32string> string_term(const SourcePrinter &printer, const ast::TermList &list) {
    if (list.elements.empty() || list.tail) {
        return std::nullopt;
    }
    std::u32string text;
    for (const auto &element : list.elements) {
        const auto *integer = std::get_if<ast::IntegerLiteral>(&printer.syntax().term(element).value);
        const auto character = integer ? printable_character(integer->value) : std::nullopt;
        if (!character) {
            return std::nullopt;
        }
        text += *character;
    }
    return text;
}

// The bytes of literal bits, a final partial byte with its bit count.
std::string bits_text(const ast::TermBits &value) {
    std::string result;
    for (std::size_t start = 0; start < value.bits.size(); start += 8) {
        const auto count = std::min<std::size_t>(8, value.bits.size() - start);
        unsigned byte = 0;
        for (std::size_t i = 0; i < count; ++i) {
            byte = (byte << 1U) | (value.bits[start + i] ? 1U : 0U);
        }
        result += (result.empty() ? "" : ", ") + std::to_string(byte) + (count == 8 ? "" : ':' + std::to_string(count));
    }
    return "<<" + result + ">>";
}

// Literal attribute data as written.
struct Term {
    const SourcePrinter &printer;

    std::string operator()(const ast::Atom &value) const { return atom_text(value); }

    std::string operator()(const ast::IntegerLiteral &value) const { return value.value.decimal; }

    std::string operator()(const ast::FloatLiteral &value) const {
        return literal_text(TokenKind::floating, value.value);
    }

    std::string operator()(const ast::TermTuple &value) const {
        std::string result;
        for (const auto &element : value.elements) {
            result += (result.empty() ? "" : ", ") + printer.term(element);
        }
        return '{' + result + '}';
    }

    std::string operator()(const ast::TermList &value) const {
        if (const auto text = string_term(printer, value)) {
            return literal_text(TokenKind::string, *text);
        }
        std::string result;
        for (const auto &element : value.elements) {
            result += (result.empty() ? "" : ", ") + printer.term(element);
        }
        return '[' + result + (value.tail ? " | " + printer.term(*value.tail) : std::string()) + ']';
    }

    std::string operator()(const ast::TermMap &value) const {
        std::string result;
        for (const auto &[key, item] : value.entries) {
            result += (result.empty() ? "" : ", ") + printer.term(key) + " => " + printer.term(item);
        }
        return "#{" + result + '}';
    }

    std::string operator()(const ast::TermBits &value) const { return bits_text(value); }

    std::string operator()(const ast::TermFunction &value) const {
        return "fun " + atom_text(value.module) + ':' + atom_text(value.name) + '/' + value.arity.decimal;
    }
};
} // namespace

std::string SourcePrinter::type(const ast::TypeId &id) const { return std::visit(Type{*this}, syntax_.type(id).value); }

std::string SourcePrinter::types(const std::vector<ast::TypeId> &items) const {
    std::string result;
    for (const auto &item : items) {
        result += (result.empty() ? "" : ", ") + type(item);
    }
    return result;
}

std::string SourcePrinter::term(const ast::TermId &id) const { return std::visit(Term{*this}, syntax_.term(id).value); }
} // namespace erlang_aot::printing
