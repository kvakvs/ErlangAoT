#include <erlang_aot/compiler/preprocessor.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Keep assertions active in optimized builds and name the failing behavior.
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

// Collect a bounded event stream; exceeding the bound exposes a recovery loop.
std::vector<PreprocessorEvent> events(std::string text) {
    SourceManager sources;
    DirectiveReader session(sources.add("forms.erl", text));
    std::vector<PreprocessorEvent> result;
    while (auto event = session.next()) {
        require(result.size() <= text.size(), "event stream failed to progress");
        result.push_back(std::move(*event));
    }
    require(!session.next(), "EOF must be stable");
    return result;
}

// Read the first directive while keeping all token source ownership in its
// value.
Directive directive(std::string text) {
    auto result = events(std::move(text));
    require(result.size() == 1, "expected one directive event");
    return std::get<Directive>(std::move(result.front()));
}

// Multiline envelopes retain incomplete bodies, empty bodies, and exact
// spellings.
void definitions() {
    auto parsed = directive("-define(\n 'NAME'(A, B),\n {A, ?B, \"-endif.\").");
    auto body = std::get<Definition>(parsed.operand);
    require(body.name.text() == U"NAME", "quoted macro name");
    require(body.parameters->size() == 2, "formal parameters");
    require(body.body.front().text() == U"{", "partial macro syntax preserved");
    require(body.body.back().text() == U"-endif.", "literal body preserved");
    require(body.name.spelling.source->spelling(body.name.spelling.begin, body.name.spelling.end) == "'NAME'",
            "owned original spelling");
    auto object = std::get<Definition>(directive("-define(X,).").operand);
    auto zero = std::get<Definition>(directive("-define(X(),).").operand);
    require(!object.parameters && object.body.empty(), "empty object body");
    require(zero.parameters && zero.parameters->empty(), "arity zero differs from object");
    directive("-define(F, fun() ->\n -include(\"text\") end).");
}

// Exercise every supported directive family without performing its later
// effects.
void directives() {
    const std::pair<const char *, DirectiveKind> cases[]{
        {"-undef(X).", DirectiveKind::undef},
        {"-ifdef(x).", DirectiveKind::ifdef},
        {"-ifndef('x').", DirectiveKind::ifndef},
        {"-if(defined(X) andalso ?COND).", DirectiveKind::if_condition},
        {"-elif(length([]) =:= 0).", DirectiveKind::elif},
        {"-else.", DirectiveKind::else_branch},
        {"-endif.", DirectiveKind::endif},
        {"-'else'.", DirectiveKind::else_branch},
        {"-include(\"a\" \".hrl\").", DirectiveKind::include},
        {"-include_lib(?PATH(app)).", DirectiveKind::include_lib},
        {"-feature(maybe_expr, disable).", DirectiveKind::feature},
        {"-error({reason, ?DETAIL}).", DirectiveKind::error},
        {"-warning(\"text\").", DirectiveKind::warning}};
    for (const auto &[text, kind] : cases) {
        require(directive(text).kind == kind, "directive kind");
    }
    auto setting = std::get<FeatureSetting>(directive("-feature(test, enable).").operand);
    require(setting.enabled && setting.name.text() == U"test", "feature value");
}

// Non-preprocessing attributes and ordinary forms keep lexical categories and
// order.
void ordinary_forms() {
    const auto result = events("-module(example). -custom({\"-define(X,1).\", 1.5}).\n"
                               "run() -> R#r.field, -error(42), ~S/-endif./. % trailing comment");
    require(result.size() == 3, "forms use lexical dots");
    for (const auto &event : result) {
        require(std::holds_alternative<OrdinaryForm>(event), "ordinary attribute passthrough");
    }
    require(std::holds_alternative<OrdinaryForm>(events("-custom(fun() ->\n -define(X,1) end).").front()),
            "unknown attribute remains opaque");
    SourceManager sources;
    const auto source = sources.add("ordinary.erl", "-custom([?X, 'if', 16#ff]).");
    auto expected = Lexer(source).form();
    DirectiveReader session(source);
    auto actual = std::get<OrdinaryForm>(*session.next()).tokens;
    require(expected.size() == actual.size(), "ordinary token count");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        require(actual[i].value == expected[i].value && actual[i].kind == expected[i].kind, "ordinary token unchanged");
        require(actual[i].spelling.begin == expected[i].spelling.begin, "ordinary location unchanged");
    }
}

// Every malformed envelope must produce an error then resume at the next form.
void malformed_envelopes() {
    const char *cases[]{"-define.",
                        "-define(1, ok).",
                        "-define(X ok).",
                        "-define(X(a), ok).",
                        "-define(X(A,), ok).",
                        "-define(X, ok.",
                        "-undef().",
                        "-undef(A,B).",
                        "-ifdef(42).",
                        "-ifndef(A) extra.",
                        "-else().",
                        "-endif(no).",
                        "-if().",
                        "-elif().",
                        "-include(42).",
                        "-include(?).",
                        "-include_lib(foo).",
                        "-feature(foo, nope).",
                        "-feature(FOO, enable).",
                        "-error().",
                        "-warning().",
                        "-define'('X,ok)."};
    for (const auto *text : cases) {
        const auto result = events(std::string(text) + "\n-ordinary(ok).");
        require(result.size() == 2, "malformed form recovery count");
        const auto &error = std::get<Diagnostic>(result[0]);
        require(error.code == DiagnosticCode::malformed_directive, text);
        require(error.primary.source && render(error).starts_with("forms.erl:1:"), "error location");
        require(std::holds_alternative<OrdinaryForm>(result[1]), "recover next form");
    }
}

// Missing dots, misplaced envelopes and scanner failures retain stable
// categories.
void diagnostics() {
    auto missing = events("-define(X, ok)");
    const auto &end = std::get<Diagnostic>(missing.front());
    require(end.code == DiagnosticCode::missing_terminator, "missing directive dot");
    require(end.primary.begin == end.primary.source->text.size(), "missing terminator at EOF");
    require(std::get<Diagnostic>(events("f() -> ok").front()).code == DiagnosticCode::missing_terminator,
            "missing ordinary dot");
    auto misplaced = events("f() ->\n  -define(X, 1).\nnext() -> ok.");
    const auto &error = std::get<Diagnostic>(misplaced.front());
    require(error.code == DiagnosticCode::misplaced_directive, "misplaced directive");
    require(error.primary.source->position(error.primary.begin).line == 2, "misplaced line");
    require(std::holds_alternative<OrdinaryForm>(misplaced.back()), "misplaced recovery");
    auto lexical = events("bad() -> 1_.\ngood() -> ok.");
    require(std::get<Diagnostic>(lexical.front()).code == DiagnosticCode::invalid_number, "lexical error category");
    require(std::holds_alternative<OrdinaryForm>(lexical.back()), "lexical recovery");
    require(!render(std::get<Diagnostic>(parse_directive({}))).empty(), "empty parser input diagnostic");
}

// Failure status and cursors are independent even when sessions are
// interleaved.
void isolation() {
    SourceManager sources;
    DirectiveReader first(sources.add("first", "-undef(). good()."));
    DirectiveReader second(sources.add("second", "-define(X, ok). ?X."));
    first.next();
    second.next();
    require(first.failed() && !second.failed(), "per-module error isolation");
    require(std::holds_alternative<OrdinaryForm>(*first.next()), "first resumes");
    const auto raw = std::get<OrdinaryForm>(*second.next());
    require(raw.tokens.front().text() == U"?", "macro expansion remains deferred");
    require(first.failed(), "recovery never clears failure");
}

// Truncated and corrupted forms terminate even when literals contain apparent
// dots.
void bounded_recovery() {
    const std::string valid = "-define(F(X), {X, ~S/-endif./, 1.0}).\n-undef(F).";
    for (std::size_t length = 0; length <= valid.size(); ++length) {
        events(valid.substr(0, length));
    }
    const char *malformed[]{"$\\x{}", "\"a\"\"b\"", "λ", "16#z", "\"unterminated", "1_ 2_ 3_"};
    for (const auto *text : malformed) {
        events(std::string(text) + ".\nnext().");
    }
}

int main() {
    definitions();
    directives();
    ordinary_forms();
    malformed_envelopes();
    diagnostics();
    isolation();
    bounded_recovery();
}
