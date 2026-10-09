#include "inference_containers.hpp"
#include "inference_values.hpp"
#include <charconv>

namespace clause::semantic::types {
namespace {
// The largest character code: a string's elements are 0..CHAR_LIMIT.
constexpr std::string_view CHAR_LIMIT = "1114111";

// One member of a fact seen as a list.
struct ListView {
    enum class Shape : std::uint8_t { nil, proper, improper, unknown, other };
    Shape shape;
    // A proper list's element or an improper list's head, and an improper list's tail.
    Id head;
    Id tail;
    // Whether a proper list is never empty.
    bool nonempty = false;
};

// A list category (list(), string(), ...) as a list; other named facts are no lists.
ListView named_view(Lattice &lattice, const Node &node) {
    const auto top = lattice.graph().top();
    if (node.name == "nonempty_improper_list" && node.children.size() == 2) {
        return {ListView::Shape::improper, node.children[0], node.children[1]};
    }
    if (node.name == "string" || node.name == "nonempty_string") {
        return {ListView::Shape::proper, lattice.range({"0", CHAR_LIMIT}), top, node.name == "nonempty_string"};
    }
    if (node.name == "maybe_improper_list" || node.name == "nonempty_maybe_improper_list") {
        return {ListView::Shape::unknown, top, top};
    }
    return {node.name == "list" ? ListView::Shape::proper : ListView::Shape::other, top, top};
}

// How a member of a fact behaves as a list.
ListView view(Lattice &lattice, const Id member) {
    const auto top = lattice.graph().top();
    const auto node = lattice.graph().get(member);
    if (member == top) {
        return {ListView::Shape::unknown, top, top};
    }
    if (node.kind == Kind::list) {
        return node.children.empty()
                   ? ListView{ListView::Shape::nil, top, top}
                   : ListView{ListView::Shape::proper, node.children[0], top, node.name == "nonempty"};
    }
    if (node.kind == Kind::application && node.module == "erlang") {
        return named_view(lattice, node);
    }
    return {ListView::Shape::other, top, top};
}

// A tuple index as a host size; none for one no tuple can have.
std::optional<std::size_t> position(const std::string_view decimal) {
    std::size_t value = 0;
    const auto parsed = std::from_chars(decimal.data(), decimal.data() + decimal.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == decimal.data() + decimal.size() && value > 0 ? std::optional{value}
                                                                                                  : std::nullopt;
}

// Whether a member is any tuple or any map: tuple(), map() or term().
bool any_of_category(const Graph &graph, const Id member, const std::string_view category) {
    const auto &node = graph.get(member);
    return member == graph.top() || (node.kind == Kind::application && node.name == category);
}

// A tuple member with element `at` (from 1) replaced by `value`, or any element possibly replaced when `at` is
// unknown; none for a member that is no tuple or too small.
std::optional<Id> replace(Lattice &lattice, const Id member, const std::optional<std::size_t> at, const Id value) {
    auto elements = lattice.graph().get(member).children;
    if (lattice.graph().get(member).kind != Kind::tuple || (at && *at > elements.size())) {
        return std::nullopt;
    }
    for (std::size_t slot = 0; slot < elements.size(); ++slot) {
        if (!at) {
            elements[slot] = lattice.join(elements[slot], value);
        } else if (slot + 1 == *at) {
            elements[slot] = value;
        }
    }
    return lattice.tuple(std::move(elements));
}

// An exact map's fields updated by `fields`; none when a := key is missing, map() for a key of several values.
std::optional<Id> update_exact(Lattice &lattice, const Id map, const std::span<const Id> fields,
                               const std::vector<bool> &exact) {
    auto children = lattice.graph().get(map).children;
    for (std::size_t field = 0; 2 * field + 1 < fields.size(); ++field) {
        const auto key = fields[2 * field];
        if (!singular(lattice.graph(), key)) {
            return lattice.category("map");
        }
        std::size_t index = 0;
        while (index < children.size() && children[index] != key) {
            index += 2;
        }
        if (index < children.size()) {
            children[index + 1] = fields[2 * field + 1];
        } else if (exact.at(field)) {
            return std::nullopt;
        } else {
            children.insert(children.end(), {key, fields[2 * field + 1]});
        }
    }
    return lattice.map(std::move(children));
}
} // namespace

Id cons(Lattice &lattice, const Cell &cell) {
    const auto [head, tail] = cell;
    std::vector<Id> results;
    for (const auto member : lattice.members(tail)) {
        const auto list = view(lattice, member);
        switch (list.shape) {
        case ListView::Shape::unknown:
            return lattice.graph().top();
        case ListView::Shape::nil:
            results.push_back(lattice.list(head, true));
            break;
        case ListView::Shape::proper:
            results.push_back(lattice.list(lattice.join(head, list.head), true));
            break;
        case ListView::Shape::improper:
            results.push_back(lattice.improper(lattice.join(head, list.head), list.tail));
            break;
        case ListView::Shape::other:
            results.push_back(lattice.improper(head, member));
            break;
        }
    }
    return lattice.join(results, 0);
}

Id append(Lattice &lattice, const Lists &lists) {
    const auto [left, right] = lists;
    std::vector<Id> results;
    for (const auto member : lattice.members(left)) {
        const auto list = view(lattice, member);
        if (list.shape == ListView::Shape::unknown) {
            return lattice.graph().top();
        }
        if (list.shape == ListView::Shape::proper) {
            results.push_back(cons(lattice, {list.head, right}));
        }
        // An empty left operand gives the right one.
        if (list.shape == ListView::Shape::nil || (list.shape == ListView::Shape::proper && !list.nonempty)) {
            results.push_back(right);
        }
    }
    return lattice.join(results, 0);
}

Id subtract(Lattice &lattice, const Id left) {
    std::vector<Id> results;
    for (const auto member : lattice.members(left)) {
        const auto list = view(lattice, member);
        if (list.shape == ListView::Shape::unknown) {
            return lattice.category("list");
        }
        if (list.shape == ListView::Shape::nil || list.shape == ListView::Shape::proper) {
            results.push_back(list.shape == ListView::Shape::nil ? lattice.nil() : lattice.list(list.head, false));
        }
    }
    return lattice.join(results, 0);
}

Id head(Lattice &lattice, const Id list) {
    std::vector<Id> results;
    for (const auto member : lattice.members(list)) {
        const auto cell = view(lattice, member);
        if (cell.shape == ListView::Shape::unknown || cell.shape == ListView::Shape::proper ||
            cell.shape == ListView::Shape::improper) {
            results.push_back(cell.head);
        }
    }
    return lattice.join(results, 0);
}

Id tail(Lattice &lattice, const Id list) {
    std::vector<Id> results;
    for (const auto member : lattice.members(list)) {
        const auto cell = view(lattice, member);
        if (cell.shape == ListView::Shape::unknown) {
            return lattice.graph().top();
        }
        if (cell.shape == ListView::Shape::proper) {
            results.push_back(lattice.list(cell.head, false));
        } else if (cell.shape == ListView::Shape::improper) {
            // The tail of [H | T] is T, or a shorter improper list when H stands for several cells.
            results.insert(results.end(), {cell.tail, member});
        }
    }
    return lattice.join(results, 0);
}

Id elements(Lattice &lattice, const Id list) { return head(lattice, list); }

Id tuple_element(Lattice &lattice, const Id tuple, const std::size_t index, const std::size_t size) {
    std::vector<Id> results;
    for (const auto member : lattice.members(tuple)) {
        if (any_of_category(lattice.graph(), member, "tuple")) {
            return lattice.graph().top();
        }
        const auto &node = lattice.graph().get(member);
        const auto count = node.children.size();
        if (node.kind == Kind::tuple && index >= 1 && (size == 0 ? count >= index : count == size)) {
            results.push_back(node.children[index - 1]);
        }
    }
    return lattice.join(results, 0);
}

Id element(Lattice &lattice, const Slot &slot) {
    const auto [index, tuple] = slot;
    const auto numbers = lattice.numbers(index);
    if (!numbers.integers) {
        return lattice.graph().bottom();
    }
    if (numbers.low && numbers.high && *numbers.low == *numbers.high) {
        const auto at = position(*numbers.low);
        return at ? tuple_element(lattice, tuple, *at) : lattice.graph().bottom();
    }
    // An unknown index reads any element.
    std::vector<Id> results;
    for (const auto member : lattice.members(tuple)) {
        if (any_of_category(lattice.graph(), member, "tuple")) {
            return lattice.graph().top();
        }
        const auto &node = lattice.graph().get(member);
        if (node.kind == Kind::tuple) {
            results.insert(results.end(), node.children.begin(), node.children.end());
        }
    }
    return lattice.join(results, 0);
}

Id set_element(Lattice &lattice, const Slot &slot, const Id value) {
    const auto [index, tuple] = slot;
    const auto numbers = lattice.numbers(index);
    const bool known = numbers.low && numbers.high && *numbers.low == *numbers.high;
    const auto at = known ? position(*numbers.low) : std::nullopt;
    std::vector<Id> results;
    for (const auto member : lattice.members(tuple)) {
        if (any_of_category(lattice.graph(), member, "tuple")) {
            return lattice.category("tuple");
        }
        if (const auto replaced = replace(lattice, member, at, value)) {
            results.push_back(*replaced);
        }
    }
    return lattice.join(results, 0);
}

Id tuple_list(Lattice &lattice, const Id tuple) {
    std::vector<Id> results;
    for (const auto member : lattice.members(tuple)) {
        if (any_of_category(lattice.graph(), member, "tuple")) {
            return lattice.category("list");
        }
        const auto node = lattice.graph().get(member);
        if (node.kind == Kind::tuple) {
            results.push_back(node.children.empty() ? lattice.nil()
                                                    : lattice.list(lattice.join(node.children, 0), true));
        }
    }
    return lattice.join(results, 0);
}

Id map_value(Lattice &lattice, const Lookup &lookup) {
    const auto [map, key] = lookup;
    const bool exact = singular(lattice.graph(), key);
    std::vector<Id> results;
    for (const auto member : lattice.members(map)) {
        if (any_of_category(lattice.graph(), member, "map")) {
            return lattice.graph().top();
        }
        const auto &node = lattice.graph().get(member);
        for (std::size_t index = 0; node.kind == Kind::map && index + 1 < node.children.size(); index += 2) {
            if (!exact || node.children[index] == key) {
                results.push_back(node.children[index + 1]);
            }
        }
    }
    return lattice.join(results, 0);
}

Id map_entries(Lattice &lattice, const Id map, const bool keys) {
    std::vector<Id> results;
    for (const auto member : lattice.members(map)) {
        if (any_of_category(lattice.graph(), member, "map")) {
            return lattice.graph().top();
        }
        const auto &node = lattice.graph().get(member);
        for (std::size_t index = keys ? 0 : 1; node.kind == Kind::map && index < node.children.size(); index += 2) {
            results.push_back(node.children[index]);
        }
    }
    return lattice.join(results, 0);
}

Id map_update(Lattice &lattice, const Id map, const std::span<const Id> fields, const std::vector<bool> &exact) {
    std::vector<Id> results;
    for (const auto member : lattice.members(map)) {
        if (any_of_category(lattice.graph(), member, "map")) {
            results.push_back(lattice.category("map"));
        } else if (lattice.graph().get(member).kind == Kind::map) {
            if (const auto updated = update_exact(lattice, member, fields, exact)) {
                results.push_back(*updated);
            }
        }
    }
    return lattice.join(results, 0);
}
} // namespace clause::semantic::types
