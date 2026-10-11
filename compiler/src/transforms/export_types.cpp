// Added for parse transforms: types, specifications and record fields as abstract format terms.
#include "export_writer.hpp"
#include <array>

namespace clause::transforms {
namespace {
struct Type {
    AbstractWriter &writer_;
    const ast::TypeSyntax &node_;

    TermId own() const { return writer_.first(node_.source); }

    // The token at an offset from the node's first token, and the one right after a child type.
    TermId at(const std::size_t offset) const { return writer_.token(node_.source, node_.source.begin + offset); }

    TermId after(const ast::TypeId &child) const {
        return writer_.token(node_.source, writer_.syntax().type(child).source.end);
    }

    Terms &terms() const { return writer_.terms(); }

    // {type, Anno, Name, Arguments}.
    TermId make(const TermId anno, const std::u32string_view name, const TermId arguments) const {
        return writer_.node(U"type", anno, {terms().atom(name), arguments});
    }

    TermId operator()(const ast::Atom &value) const { return writer_.atom_node(own(), value.name); }

    TermId operator()(const ast::Variable &value) const { return writer_.var_node(own(), value.name); }

    TermId operator()(const ast::IntegerLiteral &value) const {
        return writer_.node(U"integer", own(), {terms().integer(value.value)});
    }

    TermId operator()(const ast::CharacterLiteral &value) const {
        return writer_.node(U"char", own(), {terms().integer(static_cast<std::int64_t>(value.value))});
    }

    TermId operator()(const ast::TypeGroup &value) const { return writer_.type(value.type); }

    TermId operator()(const ast::AnnotatedType &value) const {
        const auto variable = writer_.var_node(own(), value.variable.name);
        return writer_.node(U"ann_type", own(), {terms().list({variable, writer_.type(value.type)})});
    }

    // erl_parse lifts a union on the right into one list; a parenthesized union on the left stays nested.
    TermId operator()(const ast::UnionType &value) const {
        std::vector<TermId> members{writer_.type(value.left)};
        auto right = value.right;
        while (const auto *nested = std::get_if<ast::UnionType>(&ungrouped(right).value)) {
            members.push_back(writer_.type(nested->left));
            right = nested->right;
        }
        members.push_back(writer_.type(right));
        return make(own(), U"union", terms().list(std::move(members)));
    }

    const ast::TypeSyntax &ungrouped(ast::TypeId id) const {
        while (const auto *group = std::get_if<ast::TypeGroup>(&writer_.syntax().type(id).value)) {
            id = group->type;
        }
        return writer_.syntax().type(id);
    }

    // {type, A, range, [L, R]} is annotated like its first endpoint's own node.
    TermId operator()(const ast::RangeType &value) const {
        const auto first = writer_.type(value.first);
        const auto anno = terms().node(first).children_[1];
        return make(anno, U"range", terms().list({first, writer_.type(value.last)}));
    }

    TermId operator()(const ast::UnaryType &value) const {
        const auto operation = operator_name(value.operation);
        return writer_.node(U"op", own(), {terms().atom(operation), writer_.type(value.operand)});
    }

    TermId operator()(const ast::BinaryTypeOperator &value) const {
        const auto operation = operator_name(value.operation);
        return writer_.node(U"op", after(value.left),
                            {terms().atom(operation), writer_.type(value.left), writer_.type(value.right)});
    }

    TermId operator()(const ast::TypeApplication &value) const {
        const auto arguments = writer_.types(value.arguments);
        if (value.module) {
            const auto parts = terms().list(
                {writer_.atom_node(own(), value.module->name), writer_.atom_node(at(2), value.name.name), arguments});
            return writer_.node(U"remote_type", own(), {parts});
        }
        return writer_.node(value.predefined ? U"type" : U"user_type", own(),
                            {terms().atom(value.name.name), arguments});
    }

    TermId operator()(const ast::TupleType &value) const {
        return make(own(), U"tuple", value.any ? terms().atom(U"any") : writer_.types(value.elements));
    }

    TermId operator()(const ast::ListType &value) const {
        if (!value.element) {
            return make(own(), U"nil", terms().nil());
        }
        return make(own(), value.nonempty ? U"nonempty_list" : U"list", terms().list({writer_.type(*value.element)}));
    }

    TermId operator()(const ast::MapType &value) const {
        if (value.any) {
            return make(own(), U"map", terms().atom(U"any"));
        }
        std::vector<TermId> fields;
        fields.reserve(value.fields.size());
        for (const auto &field : value.fields) {
            const auto name = field.kind == ast::MapFieldKind::exact ? U"map_field_exact" : U"map_field_assoc";
            fields.push_back(
                make(after(field.key), name, terms().list({writer_.type(field.key), writer_.type(field.value)})));
        }
        return make(own(), U"map", terms().list(std::move(fields)));
    }

    // #r{...}: the name, or {tuple, A, [Module, Name]} for #m:r{...}, then the refined field types.
    TermId operator()(const ast::RecordType &value) const {
        std::vector<TermId> members;
        if (value.module) {
            const auto parts =
                terms().list({writer_.atom_node(at(1), value.module->name), writer_.atom_node(at(3), value.name.name)});
            members.push_back(writer_.node(U"tuple", own(), {parts}));
        } else {
            members.push_back(writer_.atom_node(at(1), value.name.name));
        }
        for (const auto &field : value.fields) {
            const auto anno = writer_.first(field.source);
            members.push_back(make(anno, U"field_type",
                                   terms().list({writer_.atom_node(anno, field.name.name), writer_.type(field.type)})));
        }
        return make(own(), U"record", terms().list(std::move(members)));
    }

    // <<_:B, _:_*U>>; an omitted size is {integer, Line, 0} with a line-only annotation, as erl_parse builds it.
    TermId operator()(const ast::BitstringType &value) const {
        const auto line = terms().node(terms().node(own()).children_[0]).integer_;
        const auto zero = [&] { return writer_.node(U"integer", terms().integer(line), {terms().integer(0)}); };
        const auto base = value.base ? writer_.type(*value.base) : zero();
        const auto unit = value.unit ? writer_.type(*value.unit) : zero();
        return make(own(), U"binary", terms().list({base, unit}));
    }

    // fun() is {type, A, 'fun', []}; fun((...) -> T) and fun((Args) -> T) are annotated at the inner parenthesis.
    TermId operator()(const ast::FunType &value) const {
        if (!value.result) {
            return make(own(), U"fun", terms().nil());
        }
        return export_fun_type(writer_, at(2), value);
    }
};

// The record declaration field: {record_field, A, Name[, Default]}, wrapped as typed_record_field when typed.
TermId record_field(AbstractWriter &writer, const ast::RecordDeclarationField &field) {
    auto &terms = writer.terms();
    const auto anno = writer.first(field.source);
    std::vector<TermId> parts{writer.atom_node(anno, field.name.name)};
    if (field.default_value) {
        parts.push_back(writer.expression(*field.default_value));
    }
    const auto plain = writer.node(U"record_field", anno, std::move(parts));
    if (!field.type) {
        return plain;
    }
    return terms.tuple({terms.atom(U"typed_record_field"), plain, writer.type(*field.type)});
}

// A constraint `V :: T` (or legacy is_subtype(V, T)), annotated at its variable.
TermId constraint(AbstractWriter &writer, const ast::TypeConstraint &item) {
    auto &terms = writer.terms();
    const auto anno = writer.token(item.source, item.source.begin + (item.legacy ? 2 : 0));
    const auto parts = terms.list({writer.var_node(anno, item.variable.name), writer.type(item.bound)});
    return writer.node(U"type", anno,
                       {terms.atom(U"constraint"), terms.list({writer.atom_node(anno, U"is_subtype"), parts})});
}
} // namespace

TermId export_fun_type(AbstractWriter &writer, const TermId anno, const ast::FunType &value) {
    auto &terms = writer.terms();
    const auto arguments = value.arguments
                               ? writer.node(U"type", anno, {terms.atom(U"product"), writer.types(*value.arguments)})
                               : writer.node(U"type", anno, {terms.atom(U"any")});
    return writer.node(U"type", anno, {terms.atom(U"fun"), terms.list({arguments, writer.type(*value.result)})});
}

TermId AbstractWriter::type(const ast::TypeId &id) {
    const auto &node = syntax_.type(id);
    return std::visit(Type{*this, node}, node.value);
}

TermId export_record_fields(AbstractWriter &writer, const ast::RecordDeclaration &value) {
    std::vector<TermId> fields;
    fields.reserve(value.fields.size());
    for (const auto &field : value.fields) {
        fields.push_back(record_field(writer, field));
    }
    return writer.terms().list(std::move(fields));
}

TermId export_specification(AbstractWriter &writer, const ast::Form &form, const ast::Specification &value) {
    auto &terms = writer.terms();
    std::vector<TermId> signatures;
    signatures.reserve(value.signatures.size());
    for (const auto &signature : value.signatures) {
        const auto anno = writer.token(signature.source, signature.source.begin);
        auto function = export_fun_type(writer, anno, signature.function);
        if (!signature.constraints.empty()) {
            std::vector<TermId> constraints;
            constraints.reserve(signature.constraints.size());
            for (const auto &item : signature.constraints) {
                constraints.push_back(constraint(writer, item));
            }
            function =
                writer.node(U"type", anno,
                            {terms.atom(U"bounded_fun"), terms.list({function, terms.list(std::move(constraints))})});
        }
        signatures.push_back(function);
    }
    const auto arity = terms.integer(static_cast<std::int64_t>(value.arity));
    const auto name = value.module ? terms.tuple({terms.atom(value.module->name), terms.atom(value.name.name), arity})
                                   : terms.tuple({terms.atom(value.name.name), arity});
    const auto anno = writer.token(form.source, form.source.begin + 1);
    return writer.node(
        U"attribute", anno,
        {terms.atom(value.callback ? U"callback" : U"spec"), terms.tuple({name, terms.list(std::move(signatures))})});
}

// {attribute, A, type, {Name, Type, [Var]}}; the parameters are the variables after `-type Name(`.
TermId export_type_declaration(AbstractWriter &writer, const ast::Form &form, const ast::TypeDeclaration &value) {
    static constexpr std::array<std::u32string_view, 3> KINDS{U"type", U"opaque", U"nominal"};
    auto &terms = writer.terms();
    auto name_index = form.source.begin + 2;
    if (writer.token_text(form.source, name_index) == U"(") {
        ++name_index;
    }
    std::vector<TermId> parameters;
    for (std::size_t index = 0; index < value.parameters.size(); ++index) {
        const auto anno = writer.token(form.source, name_index + 2 + 2 * index);
        parameters.push_back(writer.var_node(anno, value.parameters[index].name));
    }
    const auto declaration =
        terms.tuple({terms.atom(value.name.name), writer.type(value.type), terms.list(std::move(parameters))});
    const auto anno = writer.token(form.source, form.source.begin + 1);
    return writer.node(U"attribute", anno, {terms.atom(KINDS.at(static_cast<std::size_t>(value.kind))), declaration});
}
} // namespace clause::transforms
