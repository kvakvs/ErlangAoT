#include "../memory/heap_object.hpp"
#include "maps.hpp"
#include "structural_order.hpp"
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
TermResult<MapPosition> map_position(std::span<const MapEntry> entries, const Term &key) {
    std::size_t first = 0;
    std::size_t last = entries.size();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto order = structural_order(entries[middle].first, key, true);
        if (!order) {
            return std::unexpected(order.error());
        }
        if (*order == 0) {
            return MapPosition{middle, true};
        }
        if (*order < 0) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }
    return MapPosition{first, false};
}

TermResult<MapEntry> map_entry(const Term &map, std::size_t index) {
    const auto size = map.map_size();
    if (!size) {
        return std::unexpected(size.error());
    }
    if (index >= *size) {
        return std::unexpected(TermError::out_of_range);
    }
    const auto object = TermAccess::object(map).value();
    const auto key = TermAccess::child(map, object.words[1 + 2 * index]);
    const auto value = TermAccess::child(map, object.words[2 + 2 * index]);
    if (!key || !value) {
        return std::unexpected(TermError::invalid_encoding);
    }
    return MapEntry{*key, *value};
}

namespace {
// Search published storage without materializing an entry vector or coercing key types.
TermResult<std::optional<Term>> find(const Term &map, const Term &key, std::size_t size) {
    const auto admitted = TermAccess::child(map, key.word());
    if (!admitted) {
        return std::unexpected(admitted.error());
    }
    std::size_t first = 0;
    std::size_t last = size;
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto entry = map_entry(map, middle).value();
        const auto order = structural_order(entry.first, *admitted, true);
        if (!order) {
            return std::unexpected(order.error());
        }
        if (*order == 0) {
            return entry.second;
        }
        if (*order < 0) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }
    return std::nullopt;
}
} // namespace
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
bool Term::is_map() const { return kind() == TermKind::map; }

TermResult<std::size_t> Term::map_size() const {
    const auto object = detail::TermAccess::object(*this);
    if (!object) {
        return std::unexpected(object.error());
    }
    if (object->kind != TermKind::map) {
        return std::unexpected(TermError::wrong_type);
    }
    return object->count;
}

TermResult<std::optional<Term>> Term::map_find(const Term &key) const {
    return map_size().and_then([&](std::size_t size) { return detail::find(*this, key, size); });
}

TermResult<bool> Term::map_contains(const Term &key) const {
    return map_find(key).transform([](const auto &value) { return value.has_value(); });
}

TermResult<std::vector<std::pair<Term, Term>>> Term::map_entries() const {
    const auto size = map_size();
    if (!size) {
        return std::unexpected(size.error());
    }
    try {
        detail::MapEntries entries;
        entries.reserve(*size);
        for (std::size_t i = 0; i < *size; ++i) {
            entries.push_back(detail::map_entry(*this, i).value());
        }
        return entries;
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}
} // namespace erlang_aot::runtime
