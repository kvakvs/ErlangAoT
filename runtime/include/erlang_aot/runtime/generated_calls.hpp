#pragma once
#include "callable.hpp"

namespace erlang_aot::runtime {
// Own one synchronous invocation's first failure, shared by all nested generated/service calls.
class GeneratedCallState final {
  public:
    // Enter a host boundary; nested entries preserve the parent's pending failure.
    bool enter() noexcept;
    // Reject raw error-service calls without a host invocation owner.
    bool active() const noexcept;
    // Clear only the outer invocation, after its host result has copied the failure.
    void leave(bool outer) noexcept;
    // Record once while active; direct host services keep their existing result contract.
    void fail(const CallFailure &failure) noexcept;
    // Convert infrastructure statuses without confusing them with Erlang errors or guard rejection.
    void fail_service(abi::v1::Status status, bool reported = false) noexcept;

    // Drop a caught Erlang exception so the invocation continues (`catch Expr`).
    void clear() noexcept { failure_.reset(); }

    // Borrow until the outer invocation ends; the payload Term is a process root (BEAM fvalue).
    const std::optional<CallFailure> &failure() const noexcept;

    // Visit the error payload word, then rebind the payload so it stays current after a collection.
    template <typename Visitor> void visit(Visitor &&visit) {
        if (failure_ && failure_->value) {
            auto word = failure_->value->word();
            visit(word);
            failure_->value->rebind(word);
        }
    }

  private:
    // Mark the host scope owning cleanup; generated calls themselves never reset this state.
    bool active_ = false;
    // First failure wins, including its Erlang payload or exact infrastructure status.
    std::optional<CallFailure> failure_;
};

// Pair every host entry with cleanup, including native exceptions and nested builtin dispatch.
class GeneratedInvocation final {
  public:
    // Join the live context's outer invocation or become its cleanup owner.
    explicit GeneratedInvocation(GeneratedCallState &state) noexcept;
    // Release the channel only when leaving its outer host boundary.
    ~GeneratedInvocation();
    GeneratedInvocation(const GeneratedInvocation &) = delete;
    GeneratedInvocation &operator=(const GeneratedInvocation &) = delete;

  private:
    // Borrow the context channel for this synchronous scope.
    GeneratedCallState &state_;
    // Only its outermost owner clears the channel on exit.
    bool outer_;
};
} // namespace erlang_aot::runtime
