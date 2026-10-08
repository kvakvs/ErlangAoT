#pragma once
#include <bit>
#include <clause/abi/equality.hpp>
#include <clause/runtime/atoms.hpp>
#include <clause/runtime/process_context.hpp>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <terms.hpp>

namespace wire {
using namespace clause::runtime;

// Leave room for generated result wrappers around the corpus's 255-cell lists while bounding test recursion.
inline constexpr unsigned transport_depth_limit = 512;

// Stable textual test transport contains values only; native words and heap addresses never cross it.
inline void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("invalid or excessive term transport");
    }
}

// Each atom spelling is decoded into the receiving runtime's owned atom storage.
inline std::string unhex(std::string_view input) {
    check(input.size() % 2 == 0);
    std::string result;
    for (std::size_t i = 0; i < input.size(); i += 2) {
        result.push_back(static_cast<char>(std::stoul(std::string(input.substr(i, 2)), nullptr, 16)));
    }
    return result;
}

// Keep the recursively defined test grammar bounded independently of runtime traversal limits.
inline Term read(ProcessContext &context, std::string_view &input, unsigned depth = 0) {
    check(!input.empty() && depth < transport_depth_limit);
    if (input.starts_with("t(") || input.starts_with("c(") || input.starts_with("m(")) {
        const bool tuple = input.front() == 't';
        const bool map = input.front() == 'm';
        input.remove_prefix(2);
        std::vector<Term> values;
        while (!input.starts_with(')')) {
            check(values.size() < 10000);
            values.push_back(read(context, input, depth + 1));
            check(!input.empty());
            if (input.front() != ',') {
                break;
            }
            input.remove_prefix(1);
        }
        check(input.starts_with(')'));
        input.remove_prefix(1);
        TermFactory factory(context);
        if (tuple) {
            return factory.tuple(values).value();
        }
        if (map) {
            std::vector<std::pair<Term, Term>> entries;
            for (const auto &entry : values) {
                check(entry.tuple_size() == 2);
                entries.emplace_back(entry.tuple_element(0).value(), entry.tuple_element(1).value());
            }
            return factory.map(entries).value();
        }
        check(values.size() == 2);
        return factory.cons(values[0], values[1]).value();
    }
    const auto end = input.find_first_of(",)");
    const auto scalar = input.substr(0, end);
    input.remove_prefix(scalar.size());
    if (scalar == "nil" || scalar == "tuple") {
        return Term::from_word(scalar == "nil" ? clause::abi::v1::empty_list : clause::abi::v1::empty_tuple).value();
    }
    if (scalar.starts_with('b')) {
        const auto colon = scalar.find(':');
        check(colon != std::string_view::npos);
        const auto count = static_cast<std::size_t>(std::stoull(std::string(scalar.substr(1, colon - 1))));
        const auto bytes = unhex(scalar.substr(colon + 1));
        return TermFactory(context).bitstring(std::as_bytes(std::span(bytes)), count).value();
    }
    if (scalar.starts_with('a')) {
        return context.atom_storage().intern(unhex(scalar.substr(1))).value();
    }
    if (scalar.starts_with('f')) {
        check(scalar.size() == 17);
        return TermFactory(context)
            .floating(std::bit_cast<double>(
                static_cast<std::uint64_t>(std::stoull(std::string(scalar.substr(1)), nullptr, 16))))
            .value();
    }
    check(scalar.starts_with('i'));
    return TermFactory(context).integer_decimal(scalar.substr(1)).value();
}

// Render nested containers by value, including arbitrary improper tails and retained error payloads.
inline void write(const Term &value, std::ostream &out = std::cout, unsigned depth = 0) {
    check(depth < transport_depth_limit);
    if (value.kind() == TermKind::empty_list) {
        out << "nil";
    } else if (value.kind() == TermKind::empty_tuple) {
        out << "tuple";
    } else if (value.is_tuple()) {
        out << "t(";
        for (std::size_t i = 0; i < value.tuple_size().value(); ++i) {
            if (i != 0) {
                out << ',';
            }
            write(value.tuple_element(i).value(), out, depth + 1);
        }
        out << ')';
    } else if (value.is_cons()) {
        out << "c(";
        write(value.head().value(), out, depth + 1);
        out << ',';
        write(value.tail().value(), out, depth + 1);
        out << ')';
    } else if (value.is_atom()) {
        static constexpr std::string_view digits = "0123456789abcdef";
        out << 'a';
        for (unsigned char byte : value.atom_utf8().value()) {
            out << digits[byte >> 4] << digits[byte & 15];
        }
    } else if (value.is_map()) {
        out << "m(";
        bool first = true;
        for (const auto &[key, entry] : value.map_entries().value()) {
            if (!first) {
                out << ',';
            }
            first = false;
            out << "t(";
            write(key, out, depth + 1);
            out << ',';
            write(entry, out, depth + 1);
            out << ')';
        }
        out << ')';
    } else if (value.is_bitstring()) {
        static constexpr std::string_view digits = "0123456789abcdef";
        out << 'b' << value.bit_size().value() << ':';
        for (const auto byte : value.bitstring_bytes().value()) {
            const auto number = std::to_integer<unsigned>(byte);
            out << digits[number >> 4] << digits[number & 15];
        }
    } else if (value.is_float()) {
        const auto flags = out.flags();
        const auto fill = out.fill();
        out << 'f' << std::hex << std::setw(16) << std::setfill('0')
            << std::bit_cast<std::uint64_t>(value.float_value().value());
        out.flags(flags);
        out.fill(fill);
    } else {
        out << 'i' << value.integer_decimal().value();
    }
}
} // namespace wire
