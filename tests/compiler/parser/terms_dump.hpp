#pragma once
#include "../encoding.hpp"
#include <bit>
#include <erlang_aot/compiler/ast/module.hpp>
#include <iostream>

// Project literal terms independently of the executable expression visitor.
struct TermDump {
    const erlang_aot::ast::Module &module;

    void child(const erlang_aot::ast::TermId &id) const { module.visit(id, *this); }

    void operator()(const erlang_aot::ast::Atom &value) const {
        std::cout << "atom\t" << test_records::hex(erlang_aot::utf8(value.name)) << '\n';
    }

    void operator()(const erlang_aot::ast::IntegerLiteral &value) const {
        std::cout << "integer\t" << value.value.decimal << '\n';
    }

    void operator()(const erlang_aot::ast::FloatLiteral &value) const {
        std::cout << "float\t" << test_records::float_bits(value.value) << '\n';
    }

    void operator()(const erlang_aot::ast::TermTuple &value) const {
        std::cout << "term_tuple\t" << value.elements.size() << '\n';
        for (const auto &id : value.elements)
            child(id);
    }

    void operator()(const erlang_aot::ast::TermList &value) const {
        for (const auto &id : value.elements) {
            std::cout << "cons\n";
            child(id);
        }
        if (value.tail)
            child(*value.tail);
        else
            std::cout << "nil\n";
    }

    void operator()(const erlang_aot::ast::TermMap &value) const {
        std::cout << "term_map\t" << value.entries.size() << '\n';
        for (const auto &[key, mapped] : value.entries) {
            child(key);
            child(mapped);
        }
    }

    void operator()(const erlang_aot::ast::TermBits &value) const {
        std::cout << "term_bits\t";
        for (bool bit : value.bits)
            std::cout << (bit ? '1' : '0');
        std::cout << '\n';
    }

    void operator()(const erlang_aot::ast::TermFunction &value) const {
        std::cout << "term_fun\n";
        (*this)(value.module);
        (*this)(value.name);
        (*this)(erlang_aot::ast::IntegerLiteral{value.arity});
    }
};
