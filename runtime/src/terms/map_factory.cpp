#include "../memory/heap_storage.hpp"
#include "maps.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
// Charge insertion moves as well as comparisons; staged construction has one shared work ceiling.
TermResult<void> associate(MapEntries &entries, const MapUpdate &update, std::size_t &budget) {
    const auto position = map_position(entries, update.key, budget);
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
    const auto cost = entries.size() - position->index + 1;
    if (cost > budget) {
        return std::unexpected(TermError::resource_limit);
    }
    budget -= cost;
    entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(position->index), {update.key, update.value});
    return {};
}

// Every child belongs to this heap/runtime before it can enter immutable map storage.
TermResult<void> validate(ProcessHeap &heap, const Term &key, const Term &value) {
    const auto checked_key = heap.add(key);
    if (!checked_key) {
        return std::unexpected(checked_key.error());
    }
    return heap.add(value).transform([](const Term &) {});
}

// Construct associations in source order, retaining the last value for each exact key.
TermResult<Term> make(ProcessHeap &heap, std::span<const MapEntry> values) {
    if (values.size() > 1'000'000) {
        return std::unexpected(TermError::resource_limit);
    }
    MapEntries entries;
    std::size_t budget = 1'000'000;
    for (const auto &[key, value] : values) {
        const auto checked = validate(heap, key, value);
        if (!checked) {
            return std::unexpected(checked.error());
        }
        const auto added = associate(entries, {key, value, false}, budget);
        if (!added) {
            return std::unexpected(added.error());
        }
    }
    return MapAccess::publish(heap, entries);
}

// Keep the source map immutable while exact updates see preceding associations from the same expression.
MapResult update(ProcessHeap &heap, const Term &map, std::span<const MapUpdate> updates) {
    const auto owned = heap.add(map);
    if (!owned) {
        return std::unexpected(MapFault{owned.error(), map});
    }
    auto entries = map.map_entries();
    if (!entries) {
        return std::unexpected(MapFault{entries.error(), map});
    }
    if (updates.size() > 1'000'000 || entries->size() > 1'000'000 - updates.size()) {
        return std::unexpected(MapFault{TermError::resource_limit, map});
    }
    std::size_t budget = 1'000'000 - entries->size();
    for (const auto &entry : updates) {
        const auto checked = validate(heap, entry.key, entry.value);
        if (!checked) {
            return std::unexpected(MapFault{checked.error(), entry.key});
        }
        const auto added = associate(*entries, entry, budget);
        if (!added) {
            return std::unexpected(MapFault{added.error(), entry.key});
        }
    }
    return MapAccess::publish(heap, *entries).transform_error([&](TermError error) { return MapFault{error, map}; });
}
} // namespace

TermResult<Term> MapAccess::publish(ProcessHeap &heap, std::span<const MapEntry> entries) {
    const auto count = 1 + entries.size() * 2;
    auto reserved = heap.reserve(count);
    if (!reserved) {
        return std::unexpected(reserved.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                            : TermError::resource_limit);
    }
    auto *words = ::new (reserved->bytes().data()) Word[count]{};
    words[0] = (static_cast<Word>(entries.size()) << layout::BoxHeader::CONTENT_SHIFT) |
               (static_cast<Word>(BoxedKind::map) << 2);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        words[1 + 2 * i] = entries[i].first.word();
        words[2 + 2 * i] = entries[i].second.word();
    }
    const auto encoded = reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed);
    const std::array objects{HeapObject{encoded, TermKind::map, {words, count}, entries.size()}};
    return detail::publish(heap.storage_, *reserved, objects);
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
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
TermResult<Term> TermFactory::map(std::span<const std::pair<Term, Term>> entries) {
    return heap().and_then([&](ProcessHeap *owner) { return detail::MapAccess::make(*owner, entries); });
}
} // namespace erlang_aot::runtime
