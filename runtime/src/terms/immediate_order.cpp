#include "immediate_order.hpp"

namespace erlang_aot::runtime::detail {
namespace {
// Preserve Erlang's admitted type order; future boxed families must extend this explicit dispatch.
std::optional<int> rank(TermKind kind) noexcept {
    switch (kind) {
    case TermKind::smallint:
        return 0;
    case TermKind::atom:
        return 1;
    case TermKind::empty_tuple:
        return 2;
    case TermKind::empty_list:
        return 3;
    default:
        return {};
    }
}

// Signed decoded payloads order mathematically, including both native integer endpoints.
TermResult<int> integers(const Term &left, const Term &right) noexcept {
    const auto lhs = left.integer_value();
    const auto rhs = right.integer_value();
    if (!lhs || !rhs) {
        return std::unexpected(TermError::invalid_encoding);
    }
    return *lhs < *rhs ? -1 : static_cast<int>(*lhs > *rhs);
}

// Valid UTF-8 byte ordering matches scalar ordering, including NUL and non-ASCII atom spellings.
TermResult<int> atoms(const Term &left, const Term &right) noexcept {
    const auto lhs = left.atom_spelling();
    const auto rhs = right.atom_spelling();
    if (!lhs || !rhs) {
        return std::unexpected(TermError::invalid_encoding);
    }
    return *lhs < *rhs ? -1 : static_cast<int>(*lhs > *rhs);
}
} // namespace

TermResult<int> immediate_order(const Term &left, const Term &right) noexcept {
    const auto lhs = rank(left.kind());
    const auto rhs = rank(right.kind());
    if (!lhs || !rhs) {
        return std::unexpected(TermError::not_implemented);
    }
    if (*lhs != *rhs) {
        return *lhs < *rhs ? -1 : 1;
    }
    if (left.kind() == TermKind::smallint) {
        return integers(left, right);
    }
    if (left.kind() == TermKind::atom) {
        return atoms(left, right);
    }
    return 0;
}
} // namespace erlang_aot::runtime::detail
