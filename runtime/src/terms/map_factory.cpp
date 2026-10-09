#include "../memory/heap_storage.hpp"
#include "maps.hpp"
#include "structural_order.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <algorithm>
#include <exception>
#include <new>
#include <stdexcept>

namespace clause::runtime::detail {
namespace {
// Most entries a map header can count: key and value words; on 32-bit targets 2^24 - 1, on 64-bit beyond memory.
constexpr std::size_t MAX_MAP_SIZE = layout::BoxHeader::MAX_COUNT / 2;

// A failed key comparison inside a sort, carried out of the comparator.
struct OrderFailure final : std::exception {
    explicit OrderFailure(TermError failure) noexcept : error(failure) {}

    TermError error;
};

// Exact key order of two entries; a failed comparison throws OrderFailure.
int key_order(const MapEntry &left, const MapEntry &right) {
    const auto order = structural_order(left.first, right.first, true);
    if (!order) {
        throw OrderFailure(order.error());
    }
    return *order;
}

// Sort entries by exact key with O(n log n) comparisons; for equal keys the first key stays with the last value,
// as successive associations would leave it.
void canonical(MapEntries &entries) {
    // Strictly ascending keys, as from a generator over sorted input, need neither sorting nor deduplication.
    const auto unordered = [](const MapEntry &a, const MapEntry &b) { return key_order(a, b) >= 0; };
    if (std::ranges::adjacent_find(entries, unordered) == entries.end()) {
        return;
    }
    std::ranges::stable_sort(entries, [](const MapEntry &a, const MapEntry &b) { return key_order(a, b) < 0; });
    std::size_t kept = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (kept != 0 && key_order(entries[kept - 1], entries[i]) == 0) {
            entries[kept - 1].second = std::move(entries[i].second);
            continue;
        }
        if (kept != i) {
            entries[kept] = std::move(entries[i]);
        }
        ++kept;
    }
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(kept), entries.end());
}

// Insert or replace one association at its exact-order slot; an exact update requires the key.
TermResult<void> associate(MapEntries &entries, const MapUpdate &update) {
    const auto position = map_position(entries, update.key);
    if (!position) {
        return std::unexpected(position.error());
    }
    if (position->found) {
        entries[position->index].second = update.value;
        return {};
    }
    if (update.exact) {
        return std::unexpected(TermError::missing_key);
    }
    entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(position->index), {update.key, update.value});
    return {};
}

// Every child belongs to this heap/runtime before it can enter immutable map storage.
TermResult<void> validate(ProcessHeap &heap, const Term &key, const Term &value) {
    const auto checked_key = heap.retain(key);
    if (!checked_key) {
        return std::unexpected(checked_key.error());
    }
    return heap.retain(value).transform([](const Term &) {});
}

// Construct associations in source order, retaining the last value for each exact key; no size or work cap.
TermResult<Term> make(ProcessHeap &heap, std::span<const MapEntry> values) {
    for (const auto &[key, value] : values) {
        if (const auto checked = validate(heap, key, value); !checked) {
            return std::unexpected(checked.error());
        }
    }
    MapEntries entries(values.begin(), values.end());
    try {
        canonical(entries);
    } catch (const OrderFailure &failure) {
        return std::unexpected(failure.error);
    }
    return MapAccess::publish(heap, entries);
}

// Keep the source map immutable while exact updates see preceding associations from the same expression.
MapResult update(ProcessHeap &heap, const Term &map, std::span<const MapUpdate> updates) {
    const auto owned = heap.retain(map);
    if (!owned) {
        return std::unexpected(MapFault{owned.error(), map});
    }
    auto entries = map.map_entries();
    if (!entries) {
        return std::unexpected(MapFault{entries.error(), map});
    }
    for (const auto &entry : updates) {
        const auto checked = validate(heap, entry.key, entry.value);
        if (!checked) {
            return std::unexpected(MapFault{checked.error(), entry.key});
        }
        const auto added = associate(*entries, entry);
        if (!added) {
            return std::unexpected(MapFault{added.error(), entry.key});
        }
    }
    return MapAccess::publish(heap, *entries).transform_error([&](TermError error) { return MapFault{error, map}; });
}
} // namespace

TermResult<Term> MapAccess::publish(ProcessHeap &heap, std::span<const MapEntry> entries) {
    if (entries.size() > MAX_MAP_SIZE) {
        return std::unexpected(TermError::resource_limit);
    }
    const auto count = 1 + entries.size() * 2;
    auto reserved = heap.reserve(count);
    if (!reserved) {
        return std::unexpected(reserved.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                            : TermError::resource_limit);
    }
    auto *words = ::new (reserved->bytes().data()) Word[count]{};
    words[0] = layout::BoxHeader::make(BoxedKind::map, count - 1);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        words[1 + 2 * i] = entries[i].first.word();
        words[2 + 2 * i] = entries[i].second.word();
    }
    return detail::publish(heap.storage_, *reserved,
                           reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed));
}

TermResult<Term> MapAccess::make(ProcessHeap &heap, std::span<const MapEntry> entries) {
    try {
        return detail::make(heap, entries);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}

MapResult MapAccess::update(ProcessHeap &heap, const Term &map, std::span<const MapUpdate> updates) {
    try {
        return detail::update(heap, map, updates);
    } catch (const std::bad_alloc &) {
        return std::unexpected(MapFault{TermError::out_of_memory, map});
    } catch (const std::length_error &) {
        return std::unexpected(MapFault{TermError::resource_limit, map});
    }
}
} // namespace clause::runtime::detail

namespace clause::runtime {
TermResult<Term> TermFactory::map(std::span<const std::pair<Term, Term>> entries) {
    return heap().and_then([&](ProcessHeap *owner) { return detail::MapAccess::make(*owner, entries); });
}
} // namespace clause::runtime
