#include "../terms/service_errors.hpp"
#include <cstdio>
#include <erlang_aot/abi/output.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <new>

namespace erlang_aot::runtime {
using abi::v1::Status;

bool write_output(const OutputSink &sink, std::string_view bytes) noexcept {
    try {
        if (sink.write) {
            return sink.write(sink.context, bytes);
        }
        return std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size();
    } catch (...) {
        return false;
    }
}

namespace detail {
namespace {
using Outcome = abi::v1::ValueOutcome;

// Render one display line and deliver it whole; failures are exact infrastructure statuses.
Status display(ProcessContext &context, Word word) {
    const auto value = Term::from_word(word, context);
    if (!value) {
        return Status::invalid_argument;
    }
    auto text = format_term(*value, TermStyle::display);
    if (!text) {
        return term_status(text.error());
    }
    text->push_back('\n');
    return write_output(context.standard_output(), *text) ? Status::ok : Status::output_failure;
}

// Contain allocation and other native exceptions at the generated-code boundary.
Status guarded_display(ProcessContext &context, Word word) noexcept {
    try {
        return display(context, word);
    } catch (const std::bad_alloc &) {
        return Status::out_of_memory;
    } catch (...) {
        return Status::internal_error;
    }
}

// Publish true only after the line was delivered; any failure enters the context channel once.
std::uint8_t display_service(ProcessContext &context, Word word, Word *output) noexcept {
    auto &state = context.generated_calls();
    if (!state.active() || state.failure()) {
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    const auto status = output ? guarded_display(context, word) : Status::invalid_argument;
    const auto result = context.atom_storage().boolean(true);
    if (status != Status::ok || !result) {
        state.fail_service(status == Status::ok ? Status::internal_error : status);
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    *output = result->word();
    return static_cast<std::uint8_t>(Outcome::success);
}
} // namespace
} // namespace detail
} // namespace erlang_aot::runtime

std::uint8_t erlang_aot_display_v1(void *context, erlang_aot::abi::v1::TermWord term,
                                   erlang_aot::abi::v1::TermWord *output) noexcept {
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::ValueOutcome::failure);
    }
    return erlang_aot::runtime::detail::display_service(*static_cast<erlang_aot::runtime::ProcessContext *>(context),
                                                        term, output);
}
