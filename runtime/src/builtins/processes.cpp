#include "typed.hpp"
#include <array>

// The process builtins of the bridge (docs/builtins.md): self/0 and make_ref/0.
namespace erlang_aot::runtime::builtins {
namespace {
// self(): the pid of the calling process.
TermResult<Term> self(ProcessContext &context) { return TermFactory(context).pid(context.identity()); }

// make_ref(): a reference unique within the program run.
TermResult<Term> make_ref(ProcessContext &context) { return TermFactory(context).make_reference(); }

constexpr std::array PROCESS_BUILTINS{
    typed_entry<self>("erlang", "self"),
    typed_entry<make_ref>("erlang", "make_ref"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> process_builtins() noexcept { return builtins::PROCESS_BUILTINS; }
} // namespace erlang_aot::runtime
