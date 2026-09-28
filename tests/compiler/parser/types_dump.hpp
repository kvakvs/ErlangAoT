#pragma once
#include "operators.hpp"
#include "terms_dump.hpp"

// Normalize only parser distinctions; expression and type IDs never interchange.
struct TypeDump {
    const erlang_aot::ast::Module &module;

    void child(const erlang_aot::ast::TypeId &id) const { module.visit(id, *this); }

    void header(std::u32string_view name, std::size_t count, std::string_view tag = "type") const {
        test_records::records.node(tag, (count), " name_hex=", test_records::hex(erlang_aot::utf8(name)),
                                   " arguments=", count);
    }

    void operator()(const erlang_aot::ast::Atom &v) const { TermDump{module}(v); }

    void operator()(const erlang_aot::ast::IntegerLiteral &v) const { TermDump{module}(v); }

    void operator()(const erlang_aot::ast::CharacterLiteral &v) const {
        test_records::records.node("char", 0, " value=", static_cast<unsigned>(v.value));
    }

    void operator()(const erlang_aot::ast::Variable &v) const {
        test_records::records.node("var", 0, " name_hex=", test_records::hex(erlang_aot::utf8(v.name)));
    }

    void operator()(const erlang_aot::ast::TypeGroup &v) const { child(v.type); }

    void operator()(const erlang_aot::ast::AnnotatedType &v) const {
        test_records::records.node("ann_type", 2);
        (*this)(v.variable);
        child(v.type);
    }

    void operator()(const erlang_aot::ast::UnionType &v) const {
        test_records::records.node("union", 2);
        child(v.left);
        child(v.right);
    }

    void operator()(const erlang_aot::ast::RangeType &v) const {
        test_records::records.node("range", 2);
        child(v.first);
        child(v.last);
    }

    void operator()(const erlang_aot::ast::UnaryType &v) const {
        test_records::records.node("unary", 1, " operator=", spelling(v.operation));
        child(v.operand);
    }

    void operator()(const erlang_aot::ast::BinaryTypeOperator &v) const {
        test_records::records.node("binary", 2, " operator=", spelling(v.operation));
        child(v.left);
        child(v.right);
    }

    void operator()(const erlang_aot::ast::TypeApplication &v) const {
        if (v.module) {
            test_records::records.node("remote", 1,
                                       " module_hex=", test_records::hex(erlang_aot::utf8(v.module->name)));
        }
        header(v.name.name, v.arguments.size(), v.predefined ? "type" : "user_type");
        for (const auto &id : v.arguments) {
            child(id);
        }
    }

    void operator()(const erlang_aot::ast::TupleType &v) const {
        if (v.any) {
            test_records::records.node("tuple_any", 0);
            return;
        }
        header(U"tuple", v.elements.size());
        for (const auto &id : v.elements) {
            child(id);
        }
    }

    void operator()(const erlang_aot::ast::ListType &v) const {
        header(v.element ? (v.nonempty ? U"nonempty_list" : U"list") : U"nil", v.element ? 1 : 0);
        if (v.element) {
            child(*v.element);
        }
    }

    void operator()(const erlang_aot::ast::MapType &v) const {
        if (v.any) {
            test_records::records.node("map_any", 0);
            return;
        }
        header(U"map", v.fields.size());
        for (const auto &f : v.fields) {
            test_records::records.node(
                "type_field", 2, " operator=", (f.kind == erlang_aot::ast::MapFieldKind::associate ? "=>" : ":="));
            child(f.key);
            child(f.value);
        }
    }

    void operator()(const erlang_aot::ast::RecordType &v) const {
        test_records::records.node(
            "record_type", (v.fields.size()),
            " module_hex=", (v.module ? test_records::hex(erlang_aot::utf8(v.module->name)) : "-"),
            " name_hex=", test_records::hex(erlang_aot::utf8(v.name.name)), " fields=", v.fields.size());
        for (const auto &f : v.fields) {
            test_records::records.node("type_field", 1, " name_hex=", test_records::hex(erlang_aot::utf8(f.name.name)));
            child(f.type);
        }
    }

    void operator()(const erlang_aot::ast::BitstringType &v) const {
        test_records::records.node("binary_type", 2);
        if (v.base) {
            child(*v.base);
        } else {
            test_records::records.node("integer", 0, " value=", 0);
        }
        if (v.unit) {
            child(*v.unit);
        } else {
            test_records::records.node("integer", 0, " value=", 0);
        }
    }

    void operator()(const erlang_aot::ast::FunType &v) const {
        test_records::records.node("fun_type", (v.arguments ? v.arguments->size() : 0) + bool(v.result), " arguments=",
                                   (v.result ? (v.arguments ? std::to_string(v.arguments->size()) : "any") : "unset"));
        if (v.arguments) {
            for (const auto &id : *v.arguments) {
                child(id);
            }
        }
        if (v.result) {
            child(*v.result);
        }
    }
};
