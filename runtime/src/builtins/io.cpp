#include "../terms/service_errors.hpp"
#include "io_format.hpp"
#include "text.hpp"
#include "typed.hpp"
#include <array>
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/output.hpp>

// The io builtins (docs/io.md): io:format/1,2 and io:put_chars/1 write UTF-8 to standard output and return ok.
// Text the standard output device rejects raises badarg (a thrown BuiltinFailure), writing nothing.
namespace erlang_aot::runtime::builtins {
namespace {
// Write `text` as UTF-8 and return ok; a rejected write is the output_failure runtime failure.
Word emit(ProcessContext &context, std::u32string_view text) {
    if (!write_output(context.standard_output(), utf8(text))) {
        context.generated_calls().fail_service(abi::v1::Status::output_failure);
        return 0;
    }
    return publish(context, context.atom_storage().intern("ok"));
}

// io:format(Format): io:format(Format, []).
Word format1(ProcessContext &context, const Term &format) { return emit(context, format_text(format, std::nullopt)); }

// io:format(Format, Args).
Word format2(ProcessContext &context, const Term &format, const Term &arguments) {
    return emit(context, format_text(format, arguments));
}

// io:put_chars(Chardata).
Word put_chars(ProcessContext &context, const Term &chardata) { return emit(context, characters(chardata)); }

constexpr std::array IO_BUILTINS{
    typed_entry<format1>("io", "format"),
    typed_entry<format2>("io", "format"),
    typed_entry<put_chars>("io", "put_chars"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> io_builtins() noexcept { return builtins::IO_BUILTINS; }
} // namespace erlang_aot::runtime
