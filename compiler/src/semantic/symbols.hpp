#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace clause::semantic {
struct SymbolIdentity {
    // UTF-8 bytes are length-independent components; arity belongs to the same identity.
    std::string module;
    std::string function;
    std::size_t arity;
    bool operator==(const SymbolIdentity &) const = default;
};

// Encode/decode the private ABI v1 symbol using disjoint separators and hexadecimal UTF-8 bytes.
std::string encode_symbol(const SymbolIdentity &identity);
std::optional<SymbolIdentity> decode_symbol(std::string_view symbol);
} // namespace clause::semantic
