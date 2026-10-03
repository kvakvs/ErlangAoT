#pragma once
#include "terms.hpp"
#include <cstddef>
#include <string>
#include <string_view>

namespace erlang_aot::runtime {
// Select OTP io_lib `~w` rules or the emulator printer behind erlang:display/1.
enum class TermStyle : std::uint8_t { write, display };

// Bound rendered text so shared subterms cannot expand into unbounded output or work.
inline constexpr std::size_t default_text_limit = std::size_t{64} << 20;

// Render one admitted term without host recursion; exceeding `limit` bytes fails with resource_limit.
TermResult<std::string> format_term(const Term &value, TermStyle style,
                                    std::size_t limit = default_text_limit) noexcept;

struct OutputSink {
    // Borrow callback state; the host keeps it alive for the runtime lifetime.
    void *context = nullptr;
    // Receive exact bytes without an added newline; null selects process stdout. False means delivery failed.
    bool (*write)(void *, std::string_view) = nullptr;
};

// Deliver every byte to the sink, or to stdout without a callback; false when any byte was rejected.
bool write_output(const OutputSink &sink, std::string_view bytes) noexcept;
} // namespace erlang_aot::runtime
