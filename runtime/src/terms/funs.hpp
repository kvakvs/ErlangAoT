#pragma once
#include "term_text.hpp"
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <span>

// Fun cells (docs/funs.md): a definition followed by the captured values of a local fun.
namespace erlang_aot::runtime::detail {
// A decoded fun, borrowing the cell's words while its Term is current.
struct FunView {
    // The definition the fun was created from; it lives as long as the runtime.
    const FunDefinition *definition;
    // Captured values in capture order; empty for an external fun.
    std::span<const Word> captures;
};

// Decode a fun; wrong_type for any other term.
TermResult<FunView> fun_view(const Term &value) noexcept;
// Print `fun M:F/A` for an external fun and #Fun<M.Index.Uniq> for a local one.
TermResult<void> print_fun(const Term &value, TermStyle style, TextOutput &out);
} // namespace erlang_aot::runtime::detail
