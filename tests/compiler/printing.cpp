#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

namespace {
// Keep round-trip checks active in optimized builds as well.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("preprocessed source printing assertion failed");
    }
}

// Compare decoded tokens, deliberately excluding physical spelling and output locations.
void same_token(const erlang_aot::Token &expected, const erlang_aot::Token &actual) {
    require(expected.kind == actual.kind);
    require(expected.value.index() == actual.value.index());
    if (const auto *integer = std::get_if<erlang_aot::Integer>(&expected.value)) {
        require(integer->decimal == std::get<erlang_aot::Integer>(actual.value).decimal);
    } else if (const auto *number = std::get_if<double>(&expected.value)) {
        require(*number == std::get<double>(actual.value));
    } else {
        require(expected.text() == actual.text());
    }
}

// Re-lex each printed form and ensure no tokens merge, disappear, or change value.
void round_trip(std::string input, erlang_aot::PreprocessorOptions options = {}) {
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession session(sources.add("input.erl", std::move(input)), std::move(options));
    while (const auto event = session.next()) {
        const auto *form = std::get_if<erlang_aot::OrdinaryForm>(&*event);
        require(form != nullptr);
        std::ostringstream output;
        erlang_aot::print_preprocessed(output, *form);
        erlang_aot::Lexer lexer(sources.add("printed.erl", output.str()));
        for (const auto &expected : form->tokens) {
            const auto actual = lexer.next();
            require(actual.has_value());
            same_token(expected, *actual);
        }
        require(!lexer.next());
    }
    require(!session.failed());
}

// Exercise raw/escaped/triple sigils, including macro-substituted bodies and suffixes.
void sigils() {
    round_trip(R"erl(-module(sigils).
-define(RAW, ~S{a\nb"c}).
-define(ID(X), X).
f() -> {?RAW, ?ID(~b"a\n\""), ~s/a\tb/, ~B"λ", ~"default\n", ~custom|raw\n|suffix}.
g() -> ~S"""
    raw\text
    """.
h() -> ~b"""
    escaped\ntext
    """.
)erl");
    round_trip("% coding: latin-1\nf() -> ~S\"\xe9\".\n");
}
} // namespace

// Cover canonical values, generated tokens, lexical boundaries, and feature-sensitive atoms.
int main() {
    round_trip(R"erl(-module(values).
-define(TEXT(X), ??X).
-define(VALUE, 16#ffff_ffff_ffff_ffff_ffff).
f(R, X) -> {?VALUE, ?LINE, ?FILE, ?MODULE, ?TEXT(a + 1),
  1.0, 1.2345678901234567e-200, 1.0e308, $\s, $\n, $\x{1f600},
  'fun', 'can\'t', 'λ', "a\\b\n\"\000λ", "one" "two",
  R#rec.field, #rec.field, R#_.field, #{key => X}, <<X:8/integer>>, + +1, - -1}.
)erl");
    erlang_aot::PreprocessorOptions options;
    options.features.emplace_back("maybe_expr", false);
    round_trip("-module(features). f() -> {maybe, else}.", std::move(options));
    sigils();
}
