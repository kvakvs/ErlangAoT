#include "process_heap.hpp"
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermResult<Term> ProcessHeap::add(const Term &value) noexcept {
    // Revalidate default slots and atom membership; no heap graph or process allocation is admitted.
    return Term::from_word(value.word(), owner_);
}

TermResult<Term> Term::copy_to(ProcessHeap &destination) const noexcept { return destination.add(*this); }
} // namespace erlang_aot::runtime
