#include "../terms/service_errors.hpp"
#include "io_format.hpp"
#include "support.hpp"
#include "text.hpp"
#include <array>
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <vector>

// The io builtins (docs/io.md): io:format/1,2 and io:put_chars/1 write UTF-8 to standard output and return ok.
// Text the standard output device rejects raises badarg, writing nothing.
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

// Produce text from the admitted arguments and write it; a FormatFailure raises its Erlang error, or records the
// failed term access.
template <typename Produce> Word output(ProcessContext &context, Arguments arguments, Produce produce) {
    std::vector<Term> terms;
    terms.reserve(arguments.size());
    for (const auto word : arguments) {
        auto term = admit(context, word);
        if (!term) {
            return 0;
        }
        terms.push_back(std::move(*term));
    }
    std::u32string text;
    try {
        text = produce(terms);
    } catch (const FormatFailure &failure) {
        if (failure.term) {
            context.generated_calls().fail_service(detail::term_status(*failure.term));
            return 0;
        }
        return raise(context, failure.reason);
    }
    return emit(context, text);
}

// io:format(Format): io:format(Format, []).
Word format1(ProcessContext &context, Arguments arguments) {
    return output(context, arguments,
                  [](const std::vector<Term> &terms) { return format_text(terms[0], std::nullopt); });
}

// io:format(Format, Args).
Word format2(ProcessContext &context, Arguments arguments) {
    return output(context, arguments, [](const std::vector<Term> &terms) { return format_text(terms[0], terms[1]); });
}

// io:put_chars(Chardata).
Word put_chars(ProcessContext &context, Arguments arguments) {
    return output(context, arguments, [](const std::vector<Term> &terms) { return characters(terms[0]); });
}

constexpr std::array IO_BUILTINS{
    BuiltinEntry{"io", "format", 1, format1},
    BuiltinEntry{"io", "format", 2, format2},
    BuiltinEntry{"io", "put_chars", 1, put_chars},
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> io_builtins() noexcept { return builtins::IO_BUILTINS; }
} // namespace erlang_aot::runtime
