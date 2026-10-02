#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermResult<bool> Term::exactly_equal(const Term &other) const {
    // Only validated immediates are admitted today; boxed equality must dispatch by representation.
    if (kind() != other.kind()) {
        return false;
    }
    if (kind() == TermKind::atom) {
        if (!is_atom() || !other.is_atom()) {
            return std::unexpected(TermError::invalid_encoding);
        }
        return atom_id() == other.atom_id();
    }
    const auto left = from_word(word());
    const auto right = from_word(other.word());
    if (!left || !right) {
        return std::unexpected(TermError::invalid_encoding);
    }
    return word() == other.word();
}
} // namespace erlang_aot::runtime

std::uint8_t erlang_aot_exact_v1(void *context, erlang_aot::abi::v1::TermWord left,
                                 erlang_aot::abi::v1::TermWord right) noexcept {
    using namespace erlang_aot;
    if (!context) {
        return static_cast<std::uint8_t>(abi::v1::Equality::failure);
    }
    auto &owner = *static_cast<runtime::ProcessContext *>(context);
    auto &state = owner.generated_calls();
    if (!state.active() || state.failure()) {
        return static_cast<std::uint8_t>(abi::v1::Equality::failure);
    }
    const auto lhs = runtime::Term::from_word(left, owner);
    const auto rhs = runtime::Term::from_word(right, owner);
    if (!lhs || !rhs) {
        state.fail_service(abi::v1::Status::invalid_argument);
        return static_cast<std::uint8_t>(abi::v1::Equality::failure);
    }
    const auto equal = lhs->exactly_equal(*rhs);
    if (!equal) {
        state.fail_service(abi::v1::Status::internal_error);
        return static_cast<std::uint8_t>(abi::v1::Equality::failure);
    }
    return static_cast<std::uint8_t>(*equal ? abi::v1::Equality::equal : abi::v1::Equality::unequal);
}
