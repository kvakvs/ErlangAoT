#pragma once
#include "lattice.hpp"
#include <span>

// Facts of containers taken apart and rebuilt (docs/semantic.md#inference): list cells, tuple elements and map
// values, for builtins, constructions and the patterns that match them. Each works member by member on a union; a
// member that would make the operation raise adds nothing.
namespace clause::semantic::types {
// A list cell [Head | Tail], and the operands of Left ++ Right.
struct Cell {
    Id head;
    Id tail;
};

struct Lists {
    Id left;
    Id right;
};

// A tuple index and the tuple it reads (element/2), with the value setelement/3 stores.
struct Slot {
    Id index;
    Id tuple;
};

// A map and the key it is read at.
struct Lookup {
    Id map;
    Id key;
};

// [Head | Tail]: a proper list when the tail is one, else an improper list.
Id cons(Lattice &lattice, const Cell &cell);
// Left ++ Right and Left -- Right.
Id append(Lattice &lattice, const Lists &lists);
Id subtract(Lattice &lattice, Id left);
// hd/1 and tl/1 of a list, and the elements a list generator visits.
Id head(Lattice &lattice, Id list);
Id tail(Lattice &lattice, Id list);
Id elements(Lattice &lattice, Id list);
// Element `index` (from 1) of the tuples of `size` elements; any size of at least `index` when `size` is 0.
Id tuple_element(Lattice &lattice, Id tuple, std::size_t index, std::size_t size = 0);
// element(N, T), setelement(N, T, V) and tuple_to_list(T).
Id element(Lattice &lattice, const Slot &slot);
Id set_element(Lattice &lattice, const Slot &slot, Id value);
Id tuple_list(Lattice &lattice, Id tuple);
// The value of `key` in a map (map_get/2 and map patterns), and the joined keys or values a map generator visits.
Id map_value(Lattice &lattice, const Lookup &lookup);
Id map_entries(Lattice &lattice, Id map, bool keys);
// A map updated with `fields` (keys and values alternate; `exact` marks each := field).
Id map_update(Lattice &lattice, Id map, std::span<const Id> fields, const std::vector<bool> &exact);
} // namespace clause::semantic::types
