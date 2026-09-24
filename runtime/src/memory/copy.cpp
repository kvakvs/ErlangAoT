#include "process_heap.hpp"

namespace erlang_aot::runtime {
TermResult<Term> ProcessHeap::add(const Term &value) noexcept {
    // Revalidate even a default-constructed slot; no roots, graph storage or runtime IDs are admitted.
    return Term::from_word(value.word());
}

TermResult<Term> Term::copy_to(ProcessHeap &destination) const noexcept { return destination.add(*this); }
} // namespace erlang_aot::runtime
