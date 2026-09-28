#pragma once
#include "../encoding.hpp"
#include "record_printer.hpp"
#include <bit>
#include <erlang_aot/compiler/ast/module.hpp>
#include <iostream>

// Project literal terms independently of the executable expression visitor.
struct TermDump {
    const erlang_aot::ast::Module &module;

    void child(const erlang_aot::ast::TermId &id) const { module.visit(id, *this); }

    void operator()(const erlang_aot::ast::Atom &value) const {
        test_records::records.node("atom", 0, " name_hex=", test_records::hex(erlang_aot::utf8(value.name)));
    }

    void operator()(const erlang_aot::ast::IntegerLiteral &value) const {
        test_records::records.node("integer", 0, " value=", value.value.decimal);
    }

    void operator()(const erlang_aot::ast::FloatLiteral &value) const {
        test_records::records.node("float", 0, " bits=", test_records::float_bits(value.value));
    }

    void operator()(const erlang_aot::ast::TermTuple &value) const {
        test_records::records.node("term_tuple", (value.elements.size()), " elements=", value.elements.size());
        for (const auto &id : value.elements) {
            child(id);
        }
    }

    void operator()(const erlang_aot::ast::TermList &value) const {
        for (const auto &id : value.elements) {
            test_records::records.node("cons", 2);
            child(id);
        }
        if (value.tail) {
            child(*value.tail);
        } else {
            test_records::records.node("nil", 0);
        }
    }

    void operator()(const erlang_aot::ast::TermMap &value) const {
        test_records::records.node("term_map", 2 * (value.entries.size()), " entries=", value.entries.size());
        for (const auto &[key, mapped] : value.entries) {
            child(key);
            child(mapped);
        }
    }

    void operator()(const erlang_aot::ast::TermBits &value) const {
        std::string bits;
        for (bool bit : value.bits) {
            bits += bit ? '1' : '0';
        }
        test_records::records.node("term_bits", 0, " bits=", bits);
    }

    void operator()(const erlang_aot::ast::TermFunction &value) const {
        test_records::records.node("term_fun", 3);
        (*this)(value.module);
        (*this)(value.name);
        (*this)(erlang_aot::ast::IntegerLiteral{value.arity});
    }
};
