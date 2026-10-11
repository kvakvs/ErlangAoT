// Added for parse transforms: expressions and patterns as abstract format terms (erl_parse.yrl expr rules).
#include "export_writer.hpp"
#include <array>

namespace clause::transforms {
namespace {
// erl_parse's operator atoms, indexed like ast::BinaryOperator and ast::UnaryOperator.
constexpr std::array<std::u32string_view, 27> BINARY{
    U"!", U"orelse", U"andalso", U"==",  U"/=",  U"=<", U"<",   U">=", U">", U"=:=", U"=/=", U"++",   U"--", U"+",
    U"-", U"bor",    U"bxor",    U"bsl", U"bsr", U"or", U"xor", U"*",  U"/", U"div", U"rem", U"band", U"and"};
constexpr std::array<std::u32string_view, 4> UNARY{U"+", U"-", U"bnot", U"not"};

// The record name of a record expression: an atom, {Module, Name}, or [] for `#_`.
TermId record_name(Terms &terms, const ast::RecordIdentity &identity) {
    if (const auto *local = std::get_if<ast::UnresolvedRecordName>(&identity.value)) {
        return terms.atom(local->name.name);
    }
    if (const auto *qualified = std::get_if<ast::QualifiedRecordName>(&identity.value)) {
        return terms.tuple({terms.atom(qualified->module.name), terms.atom(qualified->name.name)});
    }
    return terms.nil();
}

// One expression node; operands are written through the shared writer.
struct Expr {
    AbstractWriter &writer_;
    const ast::Expression &node_;

    // The annotation of the node's own first token, and of the token right after an operand.
    TermId own() const { return writer_.first(node_.source); }

    TermId after(const ast::ExprId &operand) const {
        return writer_.token(node_.source, writer_.syntax().expression(operand).source.end);
    }

    TermId make(const std::u32string_view tag, const TermId anno, std::vector<TermId> rest) const {
        return writer_.node(tag, anno, std::move(rest));
    }

    Terms &terms() const { return writer_.terms(); }

    TermId expr(const ast::ExprId &id) const { return writer_.expression(id); }

    TermId operator()(const ast::Atom &value) const { return writer_.atom_node(own(), value.name); }

    TermId operator()(const ast::Variable &value) const { return writer_.var_node(own(), value.name); }

    TermId operator()(const ast::IntegerLiteral &value) const {
        return make(U"integer", own(), {terms().integer(value.value)});
    }

    TermId operator()(const ast::FloatLiteral &value) const {
        return make(U"float", own(), {terms().floating(value.value)});
    }

    TermId operator()(const ast::CharacterLiteral &value) const {
        return make(U"char", own(), {terms().integer(static_cast<std::int64_t>(value.value))});
    }

    TermId operator()(const ast::StringLiteral &value) const {
        return make(U"string", own(), {terms().string(value.value)});
    }

    TermId operator()(const ast::Tuple &value) const {
        return make(U"tuple", own(), {writer_.expressions(value.elements)});
    }

    TermId operator()(const ast::Group &value) const { return expr(value.expression); }

    // `[A, B | T]` is {cons, '[', A, {cons, first(B), B, T}}; a missing tail is {nil, ']'}, and `[]` is {nil, '['}.
    TermId operator()(const ast::List &value) const {
        if (value.elements.empty() && !value.tail) {
            return make(U"nil", own(), {});
        }
        auto tail =
            value.tail ? expr(*value.tail) : make(U"nil", writer_.token(node_.source, node_.source.end - 1), {});
        for (std::size_t index = value.elements.size(); index-- > 0;) {
            const auto &element = value.elements[index];
            const auto anno = index == 0 ? own() : writer_.first(writer_.syntax().expression(element).source);
            tail = make(U"cons", anno, {expr(element), tail});
        }
        return tail;
    }

    TermId operator()(const ast::Bitstring &value) const {
        std::vector<TermId> segments;
        segments.reserve(value.segments.size());
        for (const auto &segment : value.segments) {
            const auto size = segment.size ? expr(*segment.size) : terms().atom(U"default");
            const auto anno = writer_.first(writer_.syntax().expression(segment.value).source);
            segments.push_back(make(U"bin_element", anno, {expr(segment.value), size, modifiers(segment)}));
        }
        return make(U"bin", own(), {terms().list(std::move(segments))});
    }

    // Type specifiers: atoms, or {Name, Value} for `unit:N`; `default` when absent.
    TermId modifiers(const ast::BinarySegment &segment) const {
        if (!segment.modifiers) {
            return terms().atom(U"default");
        }
        std::vector<TermId> result;
        result.reserve(segment.modifiers->size());
        for (const auto &modifier : *segment.modifiers) {
            const auto name = terms().atom(modifier.name.name);
            result.push_back(modifier.parameter ? terms().tuple({name, terms().integer(*modifier.parameter)}) : name);
        }
        return terms().list(std::move(result));
    }

    TermId operator()(const ast::UnaryExpression &value) const {
        const auto operation = operator_name(value.operation);
        return make(U"op", own(), {terms().atom(operation), expr(value.operand)});
    }

    TermId operator()(const ast::BinaryExpression &value) const {
        const auto operation = operator_name(value.operation);
        return make(U"op", after(value.left), {terms().atom(operation), expr(value.left), expr(value.right)});
    }

    TermId operator()(const ast::MatchExpression &value) const {
        return make(U"match", own(), {expr(value.left), expr(value.right)});
    }

    TermId operator()(const ast::CatchExpression &value) const {
        return make(U"catch", own(), {expr(value.expression)});
    }

    TermId operator()(const ast::CallExpression &value) const {
        return make(U"call", own(), {expr(value.target), writer_.expressions(value.arguments)});
    }

    TermId operator()(const ast::RemoteExpression &value) const {
        return make(U"remote", after(value.module), {expr(value.module), expr(value.function)});
    }

    TermId operator()(const ast::MapExpression &value) const {
        std::vector<TermId> fields;
        fields.reserve(value.fields.size());
        for (const auto &field : value.fields) {
            const auto tag = field.kind == ast::MapFieldKind::exact ? U"map_field_exact" : U"map_field_assoc";
            fields.push_back(make(tag, after(field.key), {expr(field.key), expr(field.value)}));
        }
        if (!value.base) {
            return make(U"map", own(), {terms().list(std::move(fields))});
        }
        return make(U"map", after(*value.base), {expr(*value.base), terms().list(std::move(fields))});
    }

    TermId operator()(const ast::RecordExpression &value) const {
        std::vector<TermId> fields;
        fields.reserve(value.fields.size());
        for (const auto &field : value.fields) {
            const auto anno = writer_.first(field.source);
            const auto *atom = std::get_if<ast::Atom>(&field.name);
            const auto name = atom ? writer_.atom_node(anno, atom->name)
                                   : writer_.var_node(anno, std::get<ast::Variable>(field.name).name);
            fields.push_back(make(U"record_field", anno, {name, expr(field.value)}));
        }
        const auto name = record_name(terms(), value.identity);
        if (!value.base) {
            return make(U"record", own(), {name, terms().list(std::move(fields))});
        }
        return make(U"record", after(*value.base), {expr(*value.base), name, terms().list(std::move(fields))});
    }

    TermId operator()(const ast::RecordAccess &value) const {
        const auto field = writer_.atom_node(writer_.first(value.field_source), value.field.name);
        return make(U"record_field", after(value.base),
                    {expr(value.base), record_name(terms(), value.identity), field});
    }

    TermId operator()(const ast::RecordIndex &value) const {
        const auto field = writer_.atom_node(writer_.first(value.field_source), value.field.name);
        return make(U"record_index", own(), {terms().atom(value.record.name), field});
    }

    TermId operator()(const ast::LocalFunReference &value) const {
        const auto function =
            terms().tuple({terms().atom(U"function"), terms().atom(value.name.name), terms().integer(value.arity)});
        return make(U"fun", own(), {function});
    }

    // `fun M:F/A`: the parts are abstract nodes at the tokens after `fun`, `:` and `/`.
    TermId operator()(const ast::RemoteFunReference &value) const {
        const auto begin = node_.source.begin;
        const auto part = [&](const std::variant<ast::Atom, ast::Variable> &item, const std::size_t offset) {
            const auto anno = writer_.token(node_.source, begin + offset);
            const auto *atom = std::get_if<ast::Atom>(&item);
            return atom ? writer_.atom_node(anno, atom->name)
                        : writer_.var_node(anno, std::get<ast::Variable>(item).name);
        };
        const auto arity_anno = writer_.token(node_.source, begin + 5);
        const auto *arity = std::get_if<Integer>(&value.arity);
        const auto arity_node = arity ? make(U"integer", arity_anno, {terms().integer(*arity)})
                                      : writer_.var_node(arity_anno, std::get<ast::Variable>(value.arity).name);
        const auto function =
            terms().tuple({terms().atom(U"function"), part(value.module, 1), part(value.name, 3), arity_node});
        return make(U"fun", own(), {function});
    }

    // Block expressions and comprehensions are written by their own category writers.
    template <typename Value> TermId operator()(const Value &) const { return export_control(writer_, node_); }
};
} // namespace

std::u32string_view operator_name(const ast::BinaryOperator operation) {
    return BINARY.at(static_cast<std::size_t>(operation));
}

std::u32string_view operator_name(const ast::UnaryOperator operation) {
    return UNARY.at(static_cast<std::size_t>(operation));
}

TermId AbstractWriter::expression(const ast::ExprId &id) {
    const auto &node = syntax_.expression(id);
    return std::visit(Expr{*this, node}, node.value);
}
} // namespace clause::transforms
