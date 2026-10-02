#include "service_errors.hpp"
#include "terms.hpp"
#include <bit>
#include <erlang_aot/abi/floats.hpp>
#include <erlang_aot/runtime/process_context.hpp>

std::uint8_t erlang_aot_float_v1(void *opaque, const char *bytes, std::size_t size,
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
    if (!bytes || !output || size != 8) {
        calls.fail_service(abi::v1::Status::invalid_argument);
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < size; ++i) {
        bits = (bits << 8) | static_cast<unsigned char>(bytes[i]);
    }
    const auto result = runtime::TermFactory(context).floating(std::bit_cast<double>(bits));
    if (!result) {
        calls.fail_service(runtime::detail::term_status(result.error()));
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    *output = result->word();
    return static_cast<std::uint8_t>(Outcome::success);
}
