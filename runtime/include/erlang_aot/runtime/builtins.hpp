#pragma once
#include "code_server.hpp"
#include <array>

namespace erlang_aot::runtime {
// Identify only explicitly reserved BIF signatures; this is not a second callable registry.
inline constexpr std::array deferred_builtins{
    FunctionRequest{"erlang", "self", 0},
    FunctionRequest{"erlang", "length", 1},
    FunctionRequest{"erlang", "spawn", 3},
    FunctionRequest{"erlang", "spawn_link", 3},
    FunctionRequest{"erlang", "send", 2},
    FunctionRequest{"erlang", "make_ref", 0},
    FunctionRequest{"erlang", "garbage_collect", 0},
    FunctionRequest{"erlang", "apply", 3},
    FunctionRequest{"erlang", "tuple_size", 1},
    FunctionRequest{"erlang", "+", 2},
};

// Match exact module/name/arity without allocation; unlisted signatures remain unknown to this skeleton.
constexpr bool is_deferred_builtin(FunctionRequest request) noexcept {
    for (const auto &entry : deferred_builtins) {
        if (entry.module == request.module && entry.function == request.function && entry.arity == request.arity) {
            return true;
        }
    }
    return false;
}
} // namespace erlang_aot::runtime
