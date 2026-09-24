#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime {
TermResult<Term> Term::from_word(Word value) noexcept {
    const auto kind = classify_immediate(value);
    if (!kind) {
        return std::unexpected(kind.error());
    }
    if (*kind != TermKind::smallint && *kind != TermKind::empty_tuple && *kind != TermKind::empty_list) {
        return std::unexpected(TermError::not_implemented);
    }
    Term result;
    result.value_ = value;
    return result;
}

Word Term::word() const noexcept { return value_; }

TermKind Term::kind() const { return TermTag{value_}.get_kind(); }

TermResult<std::int64_t> Term::integer_value() const { return decode_integer(value_); }
} // namespace erlang_aot::runtime
