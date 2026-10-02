#pragma once
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
using MapEntry = std::pair<Term, Term>;
using MapEntries = std::vector<MapEntry>;

struct MapUpdate {
    // Preserve source update order and distinguish replacement-only entries from associations.
    Term key;
    Term value;
    bool exact;
};

struct MapFault {
    // Keep the offending key/map owned until generated error handling transfers its root.
    TermError error;
    Term payload;
};

using MapResult = std::expected<Term, MapFault>;

struct MapPosition {
    // A lower-bound slot also records equality, avoiding a second potentially expensive key comparison.
    std::size_t index;
    bool found;
};

// Share bounded exact key ordering across staged updates and published immutable lookup.
TermResult<MapPosition> map_position(std::span<const MapEntry> entries, const Term &key, std::size_t &budget);
// Read one canonical pair after the owning map's shape and lifetime have been proved.
TermResult<MapEntry> map_entry(const Term &map, std::size_t index);

struct MapAccess {
    // Validate and normalize all associations before publishing a single immutable map.
    static TermResult<Term> make(ProcessHeap &heap, std::span<const MapEntry> entries);
    // Apply updates to a temporary canonical table; failure publishes no partial map.
    static MapResult update(ProcessHeap &heap, const Term &map, std::span<const MapUpdate> updates);
    // Publish only an already validated canonical table, using stable owner-indexed backing.
    static TermResult<Term> publish(ProcessHeap &heap, std::span<const MapEntry> entries);
};
} // namespace erlang_aot::runtime::detail
