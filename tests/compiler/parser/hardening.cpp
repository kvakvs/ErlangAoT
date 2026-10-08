#include <chrono>
#include <clause/compiler/parser.hpp>
#include <clause/compiler/printing.hpp>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>

using namespace clause;

// Assertions remain active in optimized, sanitizer and fuzz regression configurations.
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) {
        throw std::runtime_error("parser hardening line " + std::to_string(where.line()));
    }
}

// Return owned results after all source/preprocessing objects have been destroyed.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("hardening.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Exercise explicit EOF anchors in expanded tokens originating in an included macro.
void expanded_eof() {
    SourceManager sources;
    PreprocessorOptions options;
    options.read_file = [](const auto &) { return std::optional<std::string>{"-define(VALUE, {ok})."}; };
    PreprocessorSession pp(sources.add("expanded.erl", "-include(\"values.hrl\"). f() -> ?VALUE."), options);
    std::vector<Token> tokens;
    while (const auto event = pp.next()) {
        if (const auto *form = std::get_if<OrdinaryForm>(&*event)) {
            tokens = form->tokens;
        }
    }
    require(!pp.failed() && tokens.size() > 2);
    tokens.pop_back();
    const auto end = tokens.back();
    tokens.pop_back();
    ParserSession parser;
    parser.parse_form(tokens, end);
    const auto result = std::move(parser).finish();
    const auto &error = result.diagnostics.front();
    require(result.failed && error.expected == "'}'" && error.opener);
    require(error.location->file == "expanded.erl" && error.related.size() >= 2);
    require(std::filesystem::path(error.primary.source->name).filename() == "values.hrl");
}

// Preserve invocation coordinates, physical origins and the nearest construct opener.
void diagnostics() {
    SourceManager sources;
    Lexer lexer(sources.add("eof.erl", "f() -> (ok"));
    std::vector<Token> tokens;
    while (auto token = lexer.next()) {
        tokens.push_back(std::move(*token));
    }
    auto end = tokens.back();
    end.spelling.begin = end.spelling.end;
    end.location.column = 11;
    ParserSession session;
    session.parse_form(tokens, end);
    const auto eof = std::move(session).finish();
    require(eof.failed && eof.diagnostics.front().location->column == 11 && eof.diagnostics.front().opener);
}

// Only injected printer budgets and the parser hard ceiling remain; source stress runs through the CLI.
void stress() {
    const auto flat = parse("f() -> 0+1+2+3+4+5+6+7+8+9+10.");
    std::ostringstream tree;
    bool bounded = false;
    try {
        print_ast(tree, flat.module, 10);
    } catch (const std::length_error &) {
        bounded = true;
    }
    require(bounded);
    ParserLimits limits;
    limits.nesting = std::numeric_limits<std::size_t>::max();
    const auto hard = parse("f() -> " + std::string(2000, '(') + "ok" + std::string(2000, ')') + ".", limits);
    require(hard.failed && hard.diagnostics.front().code == DiagnosticCode::resource_limit);
}

// Repeated failures cannot reset failure state or consume unbounded diagnostics/work.
void budgets() {
    ParserLimits limits;
    limits.work = 40;
    const auto limited = parse("-custom(\"" + std::string(500, 'a') + "\").", limits);
    require(limited.failed && limited.diagnostics.back().code == DiagnosticCode::resource_limit);
    limits = {};
    limits.diagnostics = 3;
    std::string bad;
    for (int i = 0; i < 100; ++i) {
        bad += "f() -> . ";
    }
    const auto repeated = parse(bad + "good() -> ok.", limits);
    require(repeated.failed && repeated.diagnostics.size() == 4);
    limits = {};
    limits.nesting = 16;
    std::string block = "ok";
    for (int i = 0; i < 40; ++i) {
        block = "begin " + block + " end";
    }
    for (const auto &text :
         {"f(" + std::string(40, '[') + "A" + std::string(40, ']') + ") -> A.",
          "-type t() :: " + std::string(40, '[') + "atom()" + std::string(40, ']') + ".", "f() -> " + block + "."}) {
        require(parse(text, limits).failed);
    }
}

int main() {
    diagnostics();
    expanded_eof();
    stress();
    budgets();
}
