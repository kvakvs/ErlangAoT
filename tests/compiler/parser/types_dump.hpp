#pragma once
#include "operators.hpp"
#include "terms_dump.hpp"

// Normalize only parser distinctions; expression and type IDs never interchange.
struct TypeDump {
    const erlang_aot::ast::Module &module;

    void child(const erlang_aot::ast::TypeId &id) const { module.visit(id, *this); }

    void header(std::u32string_view name, std::size_t count, std::string_view tag = "type") const {
        std::cout << tag << '\t' << test_records::hex(erlang_aot::utf8(name)) << '\t' << count << '\n';
    }

    void operator()(const erlang_aot::ast::Atom &v) const { TermDump{module}(v); }

    void operator()(const erlang_aot::ast::IntegerLiteral &v) const { TermDump{module}(v); }

    void operator()(const erlang_aot::ast::CharacterLiteral &v) const {
        std::cout << "char\t" << static_cast<unsigned>(v.value) << '\n';
    }

    void operator()(const erlang_aot::ast::Variable &v) const {
        std::cout << "var\t" << test_records::hex(erlang_aot::utf8(v.name)) << '\n';
    }

    void operator()(const erlang_aot::ast::TypeGroup &v) const { child(v.type); }

    void operator()(const erlang_aot::ast::AnnotatedType &v) const {
        std::cout << "ann_type\n";
        (*this)(v.variable);
        child(v.type);
    }

    void operator()(const erlang_aot::ast::UnionType &v) const {
        std::cout << "union\n";
        child(v.left);
        child(v.right);
    }

    void operator()(const erlang_aot::ast::RangeType &v) const {
        std::cout << "range\n";
        child(v.first);
        child(v.last);
    }

    void operator()(const erlang_aot::ast::UnaryType &v) const {
        std::cout << "unary\t" << spelling(v.operation) << '\n';
        child(v.operand);
    }

    void operator()(const erlang_aot::ast::BinaryTypeOperator &v) const {
        std::cout << "binary\t" << spelling(v.operation) << '\n';
        child(v.left);
        child(v.right);
    }

    void operator()(const erlang_aot::ast::TypeApplication &v) const {
        if (v.module)
            std::cout << "remote\t" << test_records::hex(erlang_aot::utf8(v.module->name)) << '\n';
        header(v.name.name, v.arguments.size(), v.predefined ? "type" : "user_type");
        for (const auto &id : v.arguments)
            child(id);
    }

    void operator()(const erlang_aot::ast::TupleType &v) const {
        if (v.any) {
            std::cout << "tuple_any\n";
            return;
        }
        header(U"tuple", v.elements.size());
        for (const auto &id : v.elements)
            child(id);
    }

    void operator()(const erlang_aot::ast::ListType &v) const {
        header(v.element ? (v.nonempty ? U"nonempty_list" : U"list") : U"nil", v.element ? 1 : 0);
        if (v.element)
            child(*v.element);
    }

    void operator()(const erlang_aot::ast::MapType &v) const {
        if (v.any) {
            std::cout << "map_any\n";
            return;
        }
        header(U"map", v.fields.size());
        for (const auto &f : v.fields) {
            std::cout << "type_field\t" << (f.kind == erlang_aot::ast::MapFieldKind::associate ? "=>" : ":=") << '\n';
            child(f.key);
            child(f.value);
        }
    }

    void operator()(const erlang_aot::ast::RecordType &v) const {
        std::cout << "record_type\t" << (v.module ? test_records::hex(erlang_aot::utf8(v.module->name)) : "-") << '\t'
                  << test_records::hex(erlang_aot::utf8(v.name.name)) << '\t' << v.fields.size() << '\n';
        for (const auto &f : v.fields) {
            std::cout << "type_field\t" << test_records::hex(erlang_aot::utf8(f.name.name)) << '\n';
            child(f.type);
        }
    }

    void operator()(const erlang_aot::ast::BitstringType &v) const {
        std::cout << "binary_type\n";
        if (v.base)
            child(*v.base);
        else
            std::cout << "integer\t0\n";
        if (v.unit)
            child(*v.unit);
        else
            std::cout << "integer\t0\n";
    }

    void operator()(const erlang_aot::ast::FunType &v) const {
        std::cout << "fun_type\t" << (v.result ? (v.arguments ? std::to_string(v.arguments->size()) : "any") : "unset")
                  << '\n';
        if (v.arguments) {
            for (const auto &id : *v.arguments)
                child(id);
        }
        if (v.result)
            child(*v.result);
    }
};
