#pragma once
#include "status.hpp"
#include "v1.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace erlang_aot::abi::v1 {
// Dispatch synchronously through a live context; names/words borrow valid arrays (null args only at arity zero).
// Unknown signatures return unknown_builtin; recognized deferred BIFs report once and return not_implemented.
// Return explicit status and write result only on OK; errors never become words or escape as C++ exceptions.
Status dispatch_builtin(Context *context, const char *module, std::size_t module_size, const char *function,
                        std::size_t function_size, const TermWord *arguments, std::size_t arity,
                        TermWord *result) noexcept;

// One production builtin generated code reaches through the bridge (docs/builtins.md).
struct BuiltinName {
    std::string_view module;
    std::string_view function;
    std::size_t arity;
};

// Every bridge builtin. An entry's index is the `builtin` argument of erlang_aot_builtin_v1, so entries are only
// ever appended. The runtime registers an implementation for each; the compiler admits calls and funs of them.
inline constexpr std::array bridge_builtins{
    // Type tests.
    BuiltinName{"erlang", "is_atom", 1},
    BuiltinName{"erlang", "is_binary", 1},
    BuiltinName{"erlang", "is_bitstring", 1},
    BuiltinName{"erlang", "is_boolean", 1},
    BuiltinName{"erlang", "is_float", 1},
    BuiltinName{"erlang", "is_function", 1},
    BuiltinName{"erlang", "is_function", 2},
    BuiltinName{"erlang", "is_integer", 1},
    BuiltinName{"erlang", "is_list", 1},
    BuiltinName{"erlang", "is_map", 1},
    BuiltinName{"erlang", "is_number", 1},
    BuiltinName{"erlang", "is_pid", 1},
    BuiltinName{"erlang", "is_port", 1},
    BuiltinName{"erlang", "is_reference", 1},
    BuiltinName{"erlang", "is_tuple", 1},
    // Term queries and numeric conversions also legal in guards.
    BuiltinName{"erlang", "abs", 1},
    BuiltinName{"erlang", "bit_size", 1},
    BuiltinName{"erlang", "byte_size", 1},
    BuiltinName{"erlang", "ceil", 1},
    BuiltinName{"erlang", "element", 2},
    BuiltinName{"erlang", "float", 1},
    BuiltinName{"erlang", "floor", 1},
    BuiltinName{"erlang", "hd", 1},
    BuiltinName{"erlang", "length", 1},
    BuiltinName{"erlang", "map_get", 2},
    BuiltinName{"erlang", "map_size", 1},
    BuiltinName{"erlang", "is_map_key", 2},
    BuiltinName{"erlang", "max", 2},
    BuiltinName{"erlang", "min", 2},
    BuiltinName{"erlang", "round", 1},
    BuiltinName{"erlang", "size", 1},
    BuiltinName{"erlang", "tl", 1},
    BuiltinName{"erlang", "trunc", 1},
    BuiltinName{"erlang", "tuple_size", 1},
    BuiltinName{"erlang", "binary_part", 2},
    BuiltinName{"erlang", "binary_part", 3},
    // Operators as functions (fun erlang:'+'/2, apply(erlang, '<', Args)).
    BuiltinName{"erlang", "=:=", 2},
    BuiltinName{"erlang", "=/=", 2},
    BuiltinName{"erlang", "==", 2},
    BuiltinName{"erlang", "/=", 2},
    BuiltinName{"erlang", "<", 2},
    BuiltinName{"erlang", "=<", 2},
    BuiltinName{"erlang", ">", 2},
    BuiltinName{"erlang", ">=", 2},
    BuiltinName{"erlang", "not", 1},
    BuiltinName{"erlang", "and", 2},
    BuiltinName{"erlang", "or", 2},
    BuiltinName{"erlang", "xor", 2},
    BuiltinName{"erlang", "+", 2},
    BuiltinName{"erlang", "-", 2},
    BuiltinName{"erlang", "*", 2},
    BuiltinName{"erlang", "/", 2},
    BuiltinName{"erlang", "div", 2},
    BuiltinName{"erlang", "rem", 2},
    BuiltinName{"erlang", "band", 2},
    BuiltinName{"erlang", "bor", 2},
    BuiltinName{"erlang", "bxor", 2},
    BuiltinName{"erlang", "bsl", 2},
    BuiltinName{"erlang", "bsr", 2},
    BuiltinName{"erlang", "+", 1},
    BuiltinName{"erlang", "-", 1},
    BuiltinName{"erlang", "bnot", 1},
    // Body-only builtins: output, halting, raising and code introspection.
    BuiltinName{"erlang", "display", 1},
    BuiltinName{"erlang", "halt", 0},
    BuiltinName{"erlang", "halt", 1},
    BuiltinName{"erlang", "error", 1},
    BuiltinName{"erlang", "error", 2},
    BuiltinName{"erlang", "error", 3},
    BuiltinName{"erlang", "exit", 1},
    BuiltinName{"erlang", "throw", 1},
    BuiltinName{"erlang", "raise", 3},
    BuiltinName{"erlang", "function_exported", 3},
};

// The bridge index of Module:Function/Arity, if it is a bridge builtin.
constexpr std::optional<std::size_t> find_bridge_builtin(std::string_view module, std::string_view function,
                                                         std::size_t arity) noexcept {
    for (std::size_t index = 0; index < bridge_builtins.size(); ++index) {
        const auto &name = bridge_builtins[index];
        if (name.module == module && name.function == function && name.arity == arity) {
            return index;
        }
    }
    return std::nullopt;
}
} // namespace erlang_aot::abi::v1

// Call bridge builtin `builtin` (an index into bridge_builtins) with its arity of rooted `arguments` and write the
// result to `output`. An Erlang error or a failure is recorded in the checked channel; returns a Status byte.
std::uint8_t erlang_aot_builtin_v1(void *context, std::size_t builtin, const erlang_aot::abi::v1::TermWord *arguments,
                                   erlang_aot::abi::v1::TermWord *output) noexcept;
