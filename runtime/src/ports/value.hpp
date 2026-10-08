#pragma once
#include <clause/runtime/terms.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// A message a driver sends from an I/O thread, described without any heap term so it can wait while its receiver
// runs, and built in the receiver's heap when delivered (docs/ports.md#sockets).
namespace clause::runtime {
class ProcessContext;
}

namespace clause::runtime::detail {
struct PortValue final {
    enum class Kind : std::uint8_t { atom, integer, bytes, identity, tuple };
    Kind kind = Kind::atom;
    // An atom's spelling.
    std::string atom;
    std::int64_t integer = 0;
    // Bytes, as a binary or as a list of bytes.
    std::vector<std::byte> bytes;
    bool binary = false;
    // A pid or port word.
    Word identity = 0;
    std::vector<PortValue> elements;

    static PortValue of_atom(std::string name) {
        PortValue value;
        value.atom = std::move(name);
        return value;
    }

    static PortValue of_integer(std::int64_t number) {
        PortValue value;
        value.kind = Kind::integer;
        value.integer = number;
        return value;
    }

    static PortValue of_bytes(std::vector<std::byte> data, bool binary) {
        PortValue value;
        value.kind = Kind::bytes;
        value.bytes = std::move(data);
        value.binary = binary;
        return value;
    }

    static PortValue of_identity(Word word) {
        PortValue value;
        value.kind = Kind::identity;
        value.identity = word;
        return value;
    }

    static PortValue of_tuple(std::vector<PortValue> elements) {
        PortValue value;
        value.kind = Kind::tuple;
        value.elements = std::move(elements);
        return value;
    }
};

// The term of `value` built in the heap of `process`.
TermResult<Term> build_value(ProcessContext &process, const PortValue &value);
} // namespace clause::runtime::detail
