#include "heap_object.hpp"
#include "heap_storage.hpp"
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
// Erase only the newly inserted prefix, leaving earlier immutable objects and handles intact.
void unpublish(HeapStorage &storage, std::span<const HeapObject> objects, std::size_t inserted) noexcept {
    for (const auto &object : objects.first(inserted)) {
        storage.objects.erase(object.value);
    }
}
} // namespace

TermResult<Term> publish(const std::shared_ptr<HeapStorage> &storage, HeapReservation &reservation,
                         std::span<const HeapObject> objects) noexcept {
    std::size_t inserted = 0;
    auto error = TermError::out_of_memory;
    try {
        for (const auto &object : objects) {
            if (!storage->objects.emplace(object.value, object).second) {
                throw std::logic_error("duplicate unpublished heap object");
            }
            ++inserted;
        }
        const auto committed = reservation.commit();
        if (committed) {
            return TermAccess::admit(objects.front().value, storage);
        }
        error = committed.error() == HeapError::out_of_memory ? TermError::out_of_memory : TermError::resource_limit;
    } catch (const std::bad_alloc &) {
        error = TermError::out_of_memory;
    } catch (const std::length_error &) {
        error = TermError::resource_limit;
    } catch (...) {
        error = TermError::invalid_encoding;
    }
    unpublish(*storage, objects, inserted);
    return std::unexpected(error);
}
} // namespace erlang_aot::runtime::detail
