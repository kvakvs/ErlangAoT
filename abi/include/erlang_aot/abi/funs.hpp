#pragma once
#include "v1.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace erlang_aot::abi::v1 {
struct ModuleDescriptor;
struct FrameDescriptor;

// One function value a module creates (docs/funs.md), immutable beside its module descriptor: a local fun F/A or an
// anonymous fun of this module, or an external fun M:F/A. Names are atom slots of the module that registers it.
struct FunDescriptor {
    // The module creating the value; its atom slots spell the names below.
    const ModuleDescriptor *module;
    // Atom slots of the module and function the value names: this module and the function (or lambda) for a local
    // fun, M and F for an external fun.
    std::size_t module_atom;
    std::size_t function_atom;
    // The fun's Erlang arity; a local fun's code takes its captured values as further arguments.
    std::size_t arity;
    // A local fun's index among the module's funs (printed in #Fun<M.Index.Uniq>); 0 for an external fun.
    std::size_t index;
    // Nonzero for an external fun M:F/A.
    std::size_t external;
    // The code a call enters: a local fun's function, an external fun's exported function within the program, or
    // null for an external fun nothing in the program exports (calling it raises undef).
    const FrameDescriptor *frame;
};

static_assert(std::is_standard_layout_v<FunDescriptor>);
static_assert(sizeof(FunDescriptor) == 7 * sizeof(TermWord));
} // namespace erlang_aot::abi::v1

// Build a fun of `descriptor` (a FunDescriptor of a registered module) capturing `count` rooted values in
// `output`; returns a Status byte.
std::uint8_t erlang_aot_make_fun_v1(void *context, const void *descriptor,
                                    const erlang_aot::abi::v1::TermWord *captures, std::size_t count,
                                    erlang_aot::abi::v1::TermWord *output) noexcept;
// Prepare calling `fun` with the `arity` rooted arguments in `arguments` (the process registers): append its
// captured values after them and return the FrameDescriptor to enter. A non-function raises {badfun, Fun}, another
// arity {badarity, {Fun, Args}} and an external fun outside the program undef; the result is then null.
const void *erlang_aot_apply_v1(void *context, erlang_aot::abi::v1::TermWord fun, std::size_t arity,
                                erlang_aot::abi::v1::TermWord *arguments) noexcept;
