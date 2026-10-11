// Added for parse transforms: owned Erlang terms exchanged with the host OTP loader.
#include "terms.hpp"
#include <charconv>
#include <utility>

namespace clause::transforms {
namespace {
// Largest Unicode code point a character list may hold.
constexpr std::int64_t MAX_CODE_POINT = 0x10FFFF;

// Whether two leaves of the same kind hold the same value.
bool same_leaf(const TermNode &left, const TermNode &right) {
    switch (left.kind_) {
    case TermKind::atom:
        return left.atom_ == right.atom_;
    case TermKind::integer:
        return left.integer_ == right.integer_;
    case TermKind::floating:
        return left.float_ == right.float_;
    case TermKind::bits:
        return left.bit_count_ == right.bit_count_ && left.bytes_ == right.bytes_;
    default:
        return left.improper_ == right.improper_ && left.children_.size() == right.children_.size();
    }
}

// A node of `kind` with every payload empty; callers fill the one field their kind uses.
TermNode make(const TermKind kind) {
    TermNode node;
    node.kind_ = kind;
    return node;
}
} // namespace

TermId Terms::add(TermNode node) {
    if (nodes_.size() >= UINT32_MAX) {
        throw TermError("too many terms");
    }
    nodes_.push_back(std::move(node));
    return static_cast<TermId>(nodes_.size() - 1);
}

TermId Terms::atom(const std::u32string_view name) {
    auto node = make(TermKind::atom);
    node.atom_ = name;
    return add(std::move(node));
}

TermId Terms::integer(Integer value) {
    auto node = make(TermKind::integer);
    node.integer_ = std::move(value);
    return add(std::move(node));
}

TermId Terms::integer(const std::int64_t value) { return integer(Integer{std::to_string(value)}); }

TermId Terms::floating(const double value) {
    auto node = make(TermKind::floating);
    node.float_ = value;
    return add(std::move(node));
}

TermId Terms::tuple(std::vector<TermId> elements) {
    auto node = make(TermKind::tuple);
    node.children_ = std::move(elements);
    return add(std::move(node));
}

TermId Terms::list(std::vector<TermId> elements, const std::optional<TermId> tail) {
    bool improper = false;
    if (tail) {
        const auto &end = node(*tail);
        if (end.kind_ == TermKind::list) {
            elements.insert(elements.end(), end.children_.begin(), end.children_.end());
            improper = end.improper_;
        } else {
            elements.push_back(*tail);
            improper = true;
        }
    }
    auto node = make(TermKind::list);
    node.children_ = std::move(elements);
    node.improper_ = improper;
    return add(std::move(node));
}

TermId Terms::string(const std::u32string_view text) {
    std::vector<TermId> codes;
    codes.reserve(text.size());
    for (const auto code : text) {
        codes.push_back(integer(static_cast<std::int64_t>(code)));
    }
    return list(std::move(codes));
}

TermId Terms::map(std::vector<TermId> keys_and_values) {
    if (keys_and_values.size() % 2 != 0) {
        throw TermError("map needs a value for every key");
    }
    auto node = make(TermKind::map);
    node.children_ = std::move(keys_and_values);
    return add(std::move(node));
}

TermId Terms::bits(std::string bytes, const std::size_t bit_count) {
    if (bytes.size() != (bit_count + 7) / 8) {
        throw TermError("bitstring size does not match its bytes");
    }
    auto node = make(TermKind::bits);
    node.bytes_ = std::move(bytes);
    node.bit_count_ = bit_count;
    return add(std::move(node));
}

const TermNode &Terms::node(const TermId id) const {
    if (id >= nodes_.size()) {
        throw TermError("invalid term reference");
    }
    return nodes_[id];
}

bool Terms::is_atom(const TermId id, const std::u32string_view name) const {
    const auto &found = node(id);
    return found.kind_ == TermKind::atom && found.atom_ == name;
}

bool Terms::is_nil(const TermId id) const {
    const auto &found = node(id);
    return found.kind_ == TermKind::list && found.children_.empty();
}

std::optional<std::int64_t> Terms::small_integer(const TermId id) const {
    const auto &found = node(id);
    if (found.kind_ != TermKind::integer) {
        return std::nullopt;
    }
    return transforms::small_integer(found.integer_);
}

std::optional<std::int64_t> small_integer(const Integer &integer) {
    const auto &digits = integer.decimal;
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::u32string> Terms::text(const TermId id) const {
    const auto &found = node(id);
    if (found.kind_ != TermKind::list || found.improper_) {
        return std::nullopt;
    }
    std::u32string result;
    for (const auto child : found.children_) {
        const auto code = small_integer(child);
        if (!code || *code < 0 || *code > MAX_CODE_POINT) {
            return std::nullopt;
        }
        result.push_back(static_cast<char32_t>(*code));
    }
    return result;
}

bool equal(const Terms &left, const TermId left_id, const Terms &right, const TermId right_id) {
    std::vector<std::pair<TermId, TermId>> pending{{left_id, right_id}};
    while (!pending.empty()) {
        const auto [a, b] = pending.back();
        pending.pop_back();
        const auto &first = left.node(a);
        const auto &second = right.node(b);
        if (first.kind_ != second.kind_ || !same_leaf(first, second)) {
            return false;
        }
        for (std::size_t index = 0; index < first.children_.size(); ++index) {
            pending.emplace_back(first.children_[index], second.children_[index]);
        }
    }
    return true;
}
} // namespace clause::transforms
