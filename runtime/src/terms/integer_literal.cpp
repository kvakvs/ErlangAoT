#include "integers.hpp"
#include "service_errors.hpp"
#include "terms.hpp"
#include <erlang_aot/abi/integers.hpp>
#include <erlang_aot/runtime/process_context.hpp>

std::uint8_t erlang_aot_integer_v1(void *opaque, const char *digits, std::size_t size,
                                   erlang_aot::abi::v1::TermWord *output) noexcept {
    using namespace erlang_aot;
    using Outcome = abi::v1::ValueOutcome;
    if (!opaque) {
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    auto &context = *static_cast<runtime::ProcessContext *>(opaque);
    auto &calls = context.generated_calls();
    if (!calls.active() || calls.failure()) {
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    // The compiler emits only literals within the integer limit, with at most a sign on top of the digits.
    const auto too_long = size > runtime::detail::integer_decimal_limit + 1;
    if (!digits || !output || too_long) {
        calls.fail_service(too_long ? abi::v1::Status::resource_limit : abi::v1::Status::invalid_argument);
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    runtime::TermFactory factory(context);
    const auto result = factory.integer_decimal({digits, size});
    if (!result) {
        calls.fail_service(runtime::detail::term_status(result.error()));
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    *output = result->word();
    return static_cast<std::uint8_t>(Outcome::success);
}
