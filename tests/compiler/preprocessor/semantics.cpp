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

void objects() {
    auto result = run("-define(A,?B). -define(B,42). ?A. -undef(B). -define(B,7). ?A.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"42", "7"}, "source order");
    require(run("-define(A,?B). -define(B,?A). ?A.").failed, "cycle");
    require(run("-define(A,1). -define(A,2).").failed, "redefinition");
    successful(run("-undef(MISSING). ok."));
    PreprocessorOptions options;
    options.definitions = {"A", "B={123,atom}", "N=-7"};
    successful(run("{?A,?B,?N}.", options));
    require(run("?A.").failed, "module isolation");
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

void includes() {
    PreprocessorOptions options;
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
    options.limits.include_depth = 1;
    require(run("-include(\"a.hrl\").", options).diagnostics.front().code == DiagnosticCode::resource_limit,
            "include limit");
}

void branches() {
    auto result = run("-ifdef(NO). -include(\"missing\"). ?MISSING. -else. -define(X,2). -endif. ?X.");
    successful(result);
    require(integers(result) == std::vector<std::string>{"2"}, "skipped effects");
    require(run("-else.").failed, "unbalanced branch");
    require(run("-ifdef(NO).").failed, "unterminated branch");
    require(run("-ifdef(NO). -else. -else. -endif.").failed, "repeated else");
}

int main() {
    objects();
    macros();
    stringify_arguments();
    includes();
    branches();
}
