#include "service_errors.hpp"
#include "structural_order.hpp"
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermResult<bool> Term::exactly_equal(const Term &other) const {
    return detail::structural_order(*this, other, true).transform([](int order) { return order == 0; });
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
        state.fail_service(runtime::detail::term_status(equal.error()));
        return static_cast<std::uint8_t>(abi::v1::Equality::failure);
    }
    return static_cast<std::uint8_t>(*equal ? abi::v1::Equality::equal : abi::v1::Equality::unequal);
}
