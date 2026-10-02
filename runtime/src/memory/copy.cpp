#include "heap_object.hpp"
#include "process_heap.hpp"
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermResult<Term> ProcessHeap::add(const Term &value) noexcept {
    if (const auto checked = detail::TermAccess::validate(value); !checked) {
        return std::unexpected(checked.error());
    }
    // Same-heap views retain ownership; foreign compound graphs still require a future copy service.
    return Term::from_word(value.word(), owner_);
}

TermResult<Term> Term::copy_to(ProcessHeap &destination) const noexcept { return destination.add(*this); }
} // namespace erlang_aot::runtime
