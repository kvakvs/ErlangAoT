#pragma once
#include <cstdint>
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <optional>
#include <string>
#include <utility>

// io:format/1,2 and io:put_chars/1 text (docs/io.md): OTP's io_lib_format and io_lib_pretty rules applied by the
// standard output device, producing characters that the device writes as UTF-8.
namespace erlang_aot::runtime::builtins {
// Why io text could not be produced: an Erlang error to raise, or a term access failure (a runtime failure).
struct FormatFailure {
    // The Erlang error raised when `term` is empty; formatting errors are badarg like OTP's device errors.
    abi::v1::ErrorReason reason = abi::v1::ErrorReason::badarg;
    // A failed term access, reported as a service failure instead of an Erlang error.
    std::optional<TermError> term = std::nullopt;
};

// The value of a term access, or FormatFailure thrown with its error.
template <typename T> T need(TermResult<T> result) {
    if (!result) {
        throw FormatFailure{.term = result.error()};
    }
    return std::move(*result);
}

// Throw the badarg of an argument the device cannot format.
[[noreturn]] inline void bad_argument() { throw FormatFailure{}; }

// The characters of `format` with each control sequence replaced, as io_lib:format(Format, Args) (no `arguments`:
// io:format/1, Args = []); throws FormatFailure.
std::u32string format_text(const Term &format, std::optional<Term> arguments);

// unicode:characters_to_list(Chardata): code points, binaries of UTF-8 and nested lists ending in [] or a binary;
// throws FormatFailure for anything else.
std::u32string characters(const Term &chardata);

// The options of one ~p control sequence.
struct PrettyOptions {
    // Column of the first character, from 1; the field's precision or the current column.
    std::int64_t column = 1;
    // Line length the layout tries to stay within; the field width, 80 by default, 0 for one line.
    std::int64_t line_length = 80;
    // The t modifier: atoms keep characters beyond Latin-1 and UTF-8 binaries print as "..."/utf8.
    bool unicode = false;
    // Without the l modifier, printable lists and binaries print as strings.
    bool strings = true;
};

// io_lib_pretty:print/2 of `term`; throws FormatFailure.
std::u32string pretty(const Term &term, const PrettyOptions &options);
} // namespace erlang_aot::runtime::builtins
