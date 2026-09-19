#include <chrono>
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>

using namespace erlang_aot;

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
    const auto result = parse("-define(BAD, {ok, }).\n-file(\"logical.erl\",100).\nf() -> ?BAD.\ngood() -> ok.");
    require(result.failed && result.diagnostics.size() == 1);
    const auto &error = result.diagnostics.front();
    require(error.code == DiagnosticCode::parser_syntax && error.location->file == "logical.erl");
    require(error.opener && error.opener->file == "logical.erl" && !error.related.empty());
    require(render(error).find("construct opened at") != std::string::npos);
    const auto expected = parse("f() -> [a, b}.");
    require(expected.failed && expected.diagnostics.front().expected == "']'");
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

// Reject recursive input within configured limits while supporting wide and deep flat arenas.
void stress() {
    const auto start = std::chrono::steady_clock::now();
    std::string chain = "f() -> 0";
    for (int i = 0; i < 12000; ++i) {
        chain += "+1";
    }
    const auto flat = parse(chain + ".");
    require(flat.succeeded());
    std::ostringstream tree;
    print_ast(tree, flat.module);
    require(tree.str().find("[depth=") != std::string::npos);
    bool bounded = false;
    try {
        print_ast(tree, flat.module, 10);
    } catch (const std::length_error &) {
        bounded = true;
    }
    require(bounded);
    std::string invalid = "-custom(f";
    for (int i = 0; i < 12000; ++i) {
        invalid += "/1";
    }
    const auto normalized = parse(invalid + "). good() -> ok.");
    require(normalized.failed && normalized.diagnostics.front().code == DiagnosticCode::parser_syntax);
    std::string matches = "f() -> ";
    for (int i = 0; i < 2000; ++i) {
        matches += "X = ";
    }
    const auto right = parse(matches + "1.");
    require(right.failed && right.diagnostics.front().code == DiagnosticCode::resource_limit);
    ParserLimits limits;
    limits.nesting = std::numeric_limits<std::size_t>::max();
    const auto hard = parse("f() -> " + std::string(2000, '(') + "ok" + std::string(2000, ')') + ".", limits);
    require(hard.failed && hard.diagnostics.front().code == DiagnosticCode::resource_limit);
    std::cout << "12000-operator parse/print/normalize regression ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
              << '\n';
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
    std::string wide = "f() -> [";
    for (int i = 0; i < 5000; ++i) {
        wide += "1,";
    }
    require(parse(wide + "0].").succeeded());
    std::string qualifiers = "f() -> [X || X <- L";
    for (int i = 0; i < 3000; ++i) {
        qualifiers += ", true";
    }
    require(parse(qualifiers + "].").succeeded());
}

int main() {
    diagnostics();
    expanded_eof();
    stress();
    budgets();
}
