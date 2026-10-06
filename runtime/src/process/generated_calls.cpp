#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
namespace {
// Payload-bearing language failures must retain their offending term before generated root cleanup.
bool payload_reason(abi::v1::ErrorReason reason) {
    return reason == abi::v1::ErrorReason::badmatch || reason == abi::v1::ErrorReason::badarg_value ||
           reason == abi::v1::ErrorReason::badmap || reason == abi::v1::ErrorReason::badkey ||
           reason == abi::v1::ErrorReason::badrecord || reason == abi::v1::ErrorReason::case_clause ||
           (reason >= abi::v1::ErrorReason::raised_error && reason <= abi::v1::ErrorReason::bad_generators);
}

// Atom-only language failures carry no term.
bool plain_reason(abi::v1::ErrorReason reason) {
    return reason == abi::v1::ErrorReason::function_clause || reason == abi::v1::ErrorReason::badarg ||
           reason == abi::v1::ErrorReason::badarith || reason == abi::v1::ErrorReason::if_clause ||
           reason == abi::v1::ErrorReason::system_limit;
}
} // namespace

bool GeneratedCallState::enter() noexcept {
    const bool outer = !active_;
    active_ = true;
    return outer;
}

bool GeneratedCallState::active() const noexcept { return active_; }

void GeneratedCallState::leave(bool outer) noexcept {
    if (outer) {
        failure_.reset();
        active_ = false;
    }
}

void GeneratedCallState::fail(const CallFailure &failure) noexcept {
    if (active_ && !failure_) {
        failure_ = failure;
        if (failure.code == CallError::erlang_exception && !failure.stack) {
            failure_->trace = stack_.trace();
        }
    }
}

void GeneratedCallState::fail_service(abi::v1::Status status, bool reported) noexcept {
    if (status != abi::v1::Status::ok) {
        fail({.code = CallError::runtime_failure, .reported = reported, .status = status});
    }
}

const std::optional<CallFailure> &GeneratedCallState::failure() const noexcept { return failure_; }

GeneratedInvocation::GeneratedInvocation(GeneratedCallState &state) noexcept : state_(state), outer_(state.enter()) {}

GeneratedInvocation::~GeneratedInvocation() { state_.leave(outer_); }
} // namespace erlang_aot::runtime

std::uint8_t erlang_aot_call_failed_v2(void *context) noexcept {
    if (!context) {
        return 1;
    }
    const auto &state = static_cast<erlang_aot::runtime::ProcessContext *>(context)->generated_calls();
    return !state.active() || state.failure().has_value();
}

std::uint8_t erlang_aot_raise_v2(void *context, erlang_aot::abi::v1::ErrorReason reason,
                                 erlang_aot::abi::v1::TermWord value) noexcept {
    using namespace erlang_aot;
    using namespace runtime;
    if (!context) {
        return static_cast<std::uint8_t>(abi::v1::Status::invalid_argument);
    }
    auto &state = static_cast<ProcessContext *>(context)->generated_calls();
    if (!state.active()) {
        return static_cast<std::uint8_t>(abi::v1::Status::invalid_argument);
    }
    CallFailure failure{.code = CallError::erlang_exception, .reason = reason};
    if (payload_reason(reason)) {
        const auto payload = Term::from_word(value, *static_cast<ProcessContext *>(context));
        if (!payload) {
            state.fail_service(abi::v1::Status::invalid_argument);
            return static_cast<std::uint8_t>(abi::v1::Status::invalid_argument);
        }
        failure.value = *payload;
    } else if (!plain_reason(reason)) {
        state.fail_service(abi::v1::Status::invalid_argument);
        return static_cast<std::uint8_t>(abi::v1::Status::invalid_argument);
    }
    state.fail(failure);
    return 0;
}
