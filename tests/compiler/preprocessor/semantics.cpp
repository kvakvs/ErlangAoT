#include <erlang_aot/compiler/preprocessor.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot;

struct Result {
    // Capture forms and errors separately while checking the session's final status.
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
    bool failed;
};

void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

Result run(std::string text, PreprocessorOptions options = {}) {
    SourceManager sources;
    PreprocessorSession session(sources.add("module.erl", std::move(text)), std::move(options));
    Result result{{}, {}, false};
    while (auto event = session.next()) {
        if (const auto *form = std::get_if<OrdinaryForm>(&*event)) {
            if (form->tokens.size() > 1 && form->tokens[1].text() == U"file") {
                continue;
            }
            result.tokens.insert(result.tokens.end(), form->tokens.begin(), form->tokens.end());
        }
        if (auto *error = std::get_if<Diagnostic>(&*event)) {
            result.diagnostics.push_back(*error);
        }
    }
    result.failed = session.failed();
    return result;
}

std::vector<std::string> integers(const Result &result) {
    std::vector<std::string> values;
    for (const auto &token : result.tokens) {
        if (const auto *value = std::get_if<Integer>(&token.value)) {
            values.push_back(value->decimal);
        }
    }
    return values;
}

void successful(const Result &result) {
    for (const auto &error : result.diagnostics) {
        std::cerr << render(error) << '\n';
    }
    require(!result.failed, "expected successful preprocessing");
}

void macros() {
    auto result = run("-define(A, ?B). -define(B, 42). -define(F(X), {X,X}). ?F(?A). -undef(B). -define(B,7). ?A.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"42", "42", "7"}, "sequential definitions");
    result = run("-define(F(X), X). ?F(?F(3)). -define(F(), 2). -define(F, 1). {?F,?F(),?F(4)}.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"3", "1", "2", "4"}, "overloads and finite nested invocation");
    require(run("-define(A,?B). -define(B,?A). ?A.").diagnostics.front().code == DiagnosticCode::macro_cycle,
            "cycle trace");
    require(run("-define(A(X,X), X).").failed, "duplicate parameters");
    require(run("-define(A,1). -define(A,2).").failed, "redefinition");
    require(run("-undef(MISSING). ok.").failed == false, "undef missing macro");
    PreprocessorOptions options;
    options.definitions = {"A", "B={123, atom}"};
    successful(run("{?A,?B}.", options));
}

void stringify_arguments() {
    const auto result = run("-define(S(X), ??X). -define(A,42). {?S(?A + 16#ff), ?S('quoted atom'), ?S(\"str\")}.");
    successful(result);
    std::vector<std::u32string> strings;
    for (const auto &token : result.tokens) {
        if (token.kind == TokenKind::string) {
            strings.emplace_back(token.text());
        }
    }
    require(strings == std::vector<std::u32string>{U"? A + 255", U"'quoted atom'", U"\"str\""},
            "canonical raw stringification");
    const auto sigil = run("-define(S(X),??X). ?S(~b\"λ\").");
    successful(sigil);
    require(sigil.tokens.front().text() == U"b \"λ\" []", "sigil suffix stringification");
}

void conditions() {
    auto result = run("-ifdef(NO). -include(\"missing.hrl\"). ?MISSING. -else. -define(X,2). -endif. ?X.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"2"}, "inactive effects suppressed");
    const char *expressions[]{"(1 bsl 100) + 1 > (1 bsl 100)", "defined(MODULE)",
                              "not defined(MISSING)",          "1 == 1.0 andalso 1 =/= 1.0",
                              "length([1,2|[]]) =:= 2",        "map_get(key, #{key => 3}) =:= 3",
                              "element(2,{a,b}) =:= b",        "is_pid(self())",
                              "bit_size(<<1:3>>) =:= 3",       "is_integer(5,1,9)",
                              "true orelse (1 div 0 =:= 0)",   "false =:= (false andalso 1 div 0)",
                              "node() =:= nonode@nohost"};
    for (const auto *expression : expressions) {
        auto checked = run(std::string("-if(") + expression + "). yes. -else. no. -endif.");
        successful(checked);
        require(checked.tokens.front().text() == U"yes", expression);
    }
    successful(run("-if(42). no. -elif(1 div 0). no. -else. yes. -endif."));
    require(run("-if(true orelse arbitrary()). no. -endif.").failed, "short circuit still validates guard syntax");
    require(run("-else.").failed, "unbalanced conditional");
    require(run("-if(true). -else. -else. -endif.").failed, "duplicate else");
    require(run("-ifdef(MISSING).").failed, "unterminated skipped group");
}

void contextual() {
    const auto result = run("-module('quoted module').\n-define(L, ?LINE).\nf(A,B) -> "
                            "{?MODULE,?MODULE_STRING,?FUNCTION_NAME,?FUNCTION_ARITY,?L,?OTP_RELEASE,?MACHINE}.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"2", "3", "29"}, "function and source context");
    require(run("?MODULE.").failed, "module context unavailable");
    require(run("-custom(?FUNCTION_NAME).").failed, "function context unavailable");
    successful(run("-feature(maybe_expr,disable). -if(?FEATURE_AVAILABLE(maybe_expr)). yes. -endif."));
    require(run("-export([]). -feature(maybe_expr,disable).").failed, "late feature directive");
    require(run("-feature(missing,enable).").failed, "unknown feature");
    auto warning = run("-warning({hello,1}). ok.");
    require(!warning.failed && warning.diagnostics.size() == 1, "warning-only success");
    require(run("-error(problem). ok.").failed, "user error latches failure");
    require(run("-warning(1+2).").failed, "diagnostic expects term");
    const auto empty = run("-define(EMPTY,). -warning(?EMPTY).");
    require(empty.failed && empty.diagnostics.front().primary.source != nullptr,
            "empty diagnostic keeps source context");
}

void includes() {
    PreprocessorOptions options;
    std::vector<std::filesystem::path> loaded;
    options.include_loaded = [&](const auto &path) { loaded.push_back(path); };
    options.working_directory = "/virtual";
    options.read_file = [](const std::filesystem::path &path) -> std::optional<std::string> {
        if (path == "/virtual/a.hrl") {
            return "-ifndef(GUARD). -define(GUARD,true). -define(X,7). -include(\"a.hrl\"). -endif.";
        }
        if (path == "/app/include/x.hrl") {
            return "-define(APP,9).";
        }
        return {};
    };
    options.applications["test"] = "/app";
    const auto result = run("-include(\"a.hrl\"). -include_lib(\"test/include/x.hrl\"). {?X,?APP}.", options);
    successful(result);
    require(integers(result) == std::vector<std::string>{"7", "9"}, "include definitions and guards");
    require(loaded == std::vector<std::filesystem::path>{"/virtual/a.hrl", "/virtual/a.hrl", "/app/include/x.hrl"},
            "observe each loaded include, including repeated and library includes");
    options.limits.include_depth = 1;
    require(run("-include(\"a.hrl\").", options).diagnostics.front().code == DiagnosticCode::resource_limit,
            "include limit");
}

// Generate bounded inputs to exercise progress, deterministic expansion and owned source lifetimes.
void generated_inputs() {
    std::string input = "-define(M0, 0).\n";
    for (unsigned i = 1; i < 80; ++i) {
        input += "-define(M" + std::to_string(i) + ",?M" + std::to_string(i - 1) + ").\n";
    }
    input += "?M79.\n";
    const auto first = run(input);
    successful(first);
    require(integers(first) == integers(run(input)), "reproducible generated expansion");
    PreprocessorOptions limited;
    limited.limits.expansion_depth = 20;
    require(run(input, limited).diagnostics.front().code == DiagnosticCode::resource_limit, "dependency depth budget");
    limited.limits.tokens = 30;
    require(run("-define(D(X), {X,X}). ?D(?D(?D(?D(1)))).", limited).failed, "produced token budget");
    limited.limits.expression_depth = 10;
    require(run("-if((((((((((((true)))))))))))). yes. -endif.", limited).failed, "expression depth budget");
    require(run("-if((1 bsl 1000001) > 0). yes. -endif.").diagnostics.front().code == DiagnosticCode::resource_limit,
            "arithmetic resource failure stays distinct from false");
    require(run("-define(DROP(X),ok). -define(A,?DROP(?A)). ?A.").failed, "unused argument dependency cycle");
    require(run("-if(is_tuple(#r{x=1})). no. -endif.").failed,
            "controlled diagnostic for OTP record-construction crash");
    std::string nested = "-define(ON,true).\n";
    for (unsigned i = 0; i < 64; ++i) {
        nested += "-ifdef(ON).\n";
    }
    nested += "selected.\n";
    for (unsigned i = 0; i < 64; ++i) {
        nested += "-else. -error(wrong_branch). -endif.\n";
    }
    successful(run(nested));
}

// Observe logical diagnostics, exact term preservation and include/expansion origins independently of token goldens.
void locations_and_terms() {
    const auto mapped = run("-file(\"logical.erl\",100).\n?MISSING.\nok.");
    require(mapped.failed && mapped.diagnostics.front().location->file == "logical.erl", "logical diagnostic filename");
    require(mapped.diagnostics.front().location->line == 101, "logical diagnostic line");
    const auto malformed = run("-file(\"logical.erl\",100).\n-undef().");
    require(malformed.diagnostics.front().location->file == "logical.erl", "malformed directive logical filename");
    require(malformed.diagnostics.front().location->line == 101, "malformed directive logical line");
    require(mapped.tokens.front().spelling.source->name == "module.erl",
            "physical source retained after session destruction");
    const auto expanded = run("-define(A,?MISSING).\n?A.");
    require(!expanded.diagnostics.front().related.empty(), "macro diagnostic provenance");
    require(expanded.diagnostics.front().location->line == 2, "macro invocation diagnostic line");
    const auto term = run("-warning(#{a => <<1,2,3:2>>, b => [1|tail]}). -warning(fun erlang:length/1).");
    require(!term.failed && term.diagnostics.size() == 2, "compound diagnostic terms");
    require(term.diagnostics[0].message.find("=>") != std::string::npos, "map term retained");
    require(term.diagnostics[0].message.find("<<") != std::string::npos, "bitstring term retained");
    require(term.diagnostics[1].message.find("fun") != std::string::npos, "external fun term retained");
    const auto functions = run("-warning(#{fun erlang:length/1 => a, fun erlang:abs/1 => b}).");
    require(functions.diagnostics.front().message.find("length") != std::string::npos &&
                functions.diagnostics.front().message.find("abs") != std::string::npos,
            "distinct external fun map keys");
    PreprocessorOptions options;
    options.definitions = {"N=-123", "S=[97,98]"};
    const auto predefined = run("{?N,?S}.", options);
    successful(predefined);
    require(integers(predefined) == std::vector<std::string>{"-123"}, "initial numeric value normalized");
    require(run("?N.").failed, "session definitions remain isolated");
}

// Lookup policy remains deterministic with competing directories and an injected environment.
void include_policy() {
    PreprocessorOptions options;
    options.working_directory = "/virtual";
    options.include_paths = {"first", "second"};
    options.environment = [](std::string_view name) -> std::optional<std::string> {
        return name == "HDR" ? std::optional<std::string>("/env") : std::nullopt;
    };
    options.read_file = [](const std::filesystem::path &path) -> std::optional<std::string> {
        if (path == "/virtual/first/x.hrl") {
            return "-define(X,1).";
        }
        if (path == "/virtual/second/x.hrl") {
            return "-define(X,2).";
        }
        if (path == "/env/x.hrl") {
            return "-define(E,3).";
        }
        if (path == "/virtual/bad.hrl") {
            return "-error(included).";
        }
        return {};
    };
    const auto result = run("-include(\"x.hrl\"). -include(\"$HDR/x.hrl\"). {?X,?E}.", options);
    successful(result);
    require(integers(result) == std::vector<std::string>{"1", "3"}, "include ordering and environment substitution");
    const auto bad = run("-include(\"bad.hrl\").", options);
    require(bad.failed && !bad.diagnostics.front().related.empty(), "include trace");
    options.read_file = [](const std::filesystem::path &path) -> std::optional<std::string> {
        if (path.filename() == "latin.hrl") {
            return std::string("% coding: latin-1\n-define(LATIN,'caf") + char(0xe9) + "').";
        }
        if (path.filename() == "feature.hrl") {
            return "-feature(maybe_expr,disable).";
        }
        return std::string(1, char(0xff));
    };
    const auto encoded = run("-include(\"latin.hrl\"). ?LATIN.", options);
    successful(encoded);
    require(encoded.tokens.front().text() == U"café", "include independently decoded");
    require(run("-include(\"invalid.hrl\").", options).failed, "invalid included encoding");
    const auto feature = run("-include(\"feature.hrl\"). {maybe,else}.", options);
    successful(feature);
    require(feature.tokens[1].kind == TokenKind::atom, "included feature affects parent keywords");
    options.read_file = [](const std::filesystem::path &) -> std::optional<std::string> {
        throw std::runtime_error("inactive include accessed filesystem");
    };
    successful(run("-if(false). -include(\"missing\"). -endif.", options));
}

int main() {
    macros();
    stringify_arguments();
    conditions();
    contextual();
    includes();
    generated_inputs();
    locations_and_terms();
    include_policy();
}
