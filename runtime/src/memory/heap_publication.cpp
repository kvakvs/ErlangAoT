#include "heap_object.hpp"
#include "heap_storage.hpp"

namespace clause::runtime::detail {
TermResult<Term> publish(const std::shared_ptr<HeapStorage> &storage, HeapReservation &reservation,
                         Word value) noexcept {
    const auto committed = reservation.commit();
    if (!committed) {
        return std::unexpected(committed.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                             : TermError::resource_limit);
    }
    return TermAccess::admit(value, *storage);
}
} // namespace clause::runtime::detail
