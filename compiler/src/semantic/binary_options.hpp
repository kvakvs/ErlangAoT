#pragma once
#include <erlang_aot/abi/bits.hpp>
#include <erlang_aot/compiler/ast/expressions.hpp>

namespace erlang_aot::semantic {
struct BinaryOptions {
    // Canonical defaults are target-independent; native endian is resolved during target lowering.
    abi::v1::BitType type = abi::v1::BitType::integer;
    unsigned unit = 1;
    unsigned size = 8;
    bool little = false;
    bool native = false;
    bool signed_value = false;
    bool all = false;
};

// Normalize already validated modifiers once for construction and pattern-plan consumers.
BinaryOptions binary_options(const ast::BinarySegment &segment);
} // namespace erlang_aot::semantic
