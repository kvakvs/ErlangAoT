#include <algorithm>
#include <bit>
#include <erlang_aot/compiler/parser.hpp>
#include <source_location>
#include <stdexcept>

using namespace erlang_aot;
namespace ast = erlang_aot::ast;

// Keep API regressions active in every build configuration.
void require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error("expanded form parser check failed at line " + std::to_string(location.line()));
    }
}

// Run the real native preprocessing/parsing pipeline with short-lived sources.
ParseResult parse(std::string text, PreprocessorOptions options = {}, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("main.erl", std::move(text)), std::move(options));
    return parse_module(pp, limits);
}

// Find a named function without assuming how many implicit file attributes were emitted.
const ast::Function &function(const ast::Module &module, std::u32string_view name) {
    for (const auto &id : module.forms()) {
        const auto *value = std::get_if<ast::Function>(&module.form(id).value);
        if (value && value->name.name == name) {
            return *value;
        }
    }
    throw std::runtime_error("missing parsed function");
}

// Inspect the single scalar expression supported by Phase I function bodies.
const ast::Expression &body(const ast::Module &module, std::u32string_view name) {
    return module.expression(function(module, name).clauses.front().body.front());
}

// Preserve scalar categories, precision, and decoded character/string values.
void scalars() {
    const auto result = parse("-module(m).\ni() -> 123456789012345678901234567890.\n"
                              "a() -> 'end'.\nf() -> 1.25.\nc() -> $λ.\ns() -> \"hello λ\".\n");
    require(result.succeeded() && result.diagnostics.empty());
    require(std::get<ast::IntegerLiteral>(body(result.module, U"i").value).value.decimal ==
            "123456789012345678901234567890");
    require(std::get<ast::Atom>(body(result.module, U"a").value).name == U"end");
    require(std::bit_cast<std::uint64_t>(std::get<ast::FloatLiteral>(body(result.module, U"f").value).value) ==
            std::bit_cast<std::uint64_t>(1.25));
    require(std::get<ast::CharacterLiteral>(body(result.module, U"c").value).value == U'λ');
    require(std::get<ast::StringLiteral>(body(result.module, U"s").value).value == U"hello λ");
}

// Distinguish syntax errors, pending grammar, warnings, and preprocessor failures.
void recovery() {
    const auto result = parse("-module(m).\nbad() -> .\n-pending(run()).\ngood() -> 42.\n");
    require(result.failed && result.diagnostics.size() == 2);
    require(result.diagnostics[0].code == DiagnosticCode::parser_syntax);
    require(result.diagnostics[1].code == DiagnosticCode::parser_syntax);
    require(result.module.term_count() == 0);
    require(std::get<ast::IntegerLiteral>(body(result.module, U"good").value).value.decimal == "42");
    const auto warning = parse("-warning(hello).\ngood() -> ok.\n");
    require(warning.succeeded() && warning.diagnostics.size() == 1);
    const auto preprocessing = parse("bad() -> ?MISSING.\ngood() -> ok.\n");
    require(preprocessing.failed && preprocessing.diagnostics[0].code == DiagnosticCode::undefined_macro);
    require(function(preprocessing.module, U"good").clauses.front().body.size() == 1);
}

// Build raw tokens for entry-point contract and exact form-boundary checks.
std::vector<Token> scan(std::string text) {
    SourceManager sources;
    Lexer lexer(sources.add("raw.erl", std::move(text)));
    std::vector<Token> result;
    while (auto token = lexer.next()) {
        result.push_back(std::move(*token));
    }
    return result;
}

// Missing dots, trailing forms, and broken token values roll back every allocation.
void raw_forms() {
    ParserSession parser;
    auto input = scan("f() -> 42.");
    auto extra = scan("g() -> ok.");
    input.insert(input.end(), extra.begin(), extra.end());
    parser.parse_form(input, input.back());
    auto missing = scan("f() -> 42");
    parser.parse_form(missing, missing.back());
    auto invalid = scan("f() -> 42.");
    invalid[4].value = std::u32string(U"invalid");
    parser.parse_form(invalid, invalid.back());
    Token eof{TokenKind::dot, std::u32string{}, {}, {"empty.erl", 1, 1}, {}};
    parser.parse_form({}, eof);
    parser.parse_form(extra, extra.back());
    const auto result = std::move(parser).finish();
    require(result.failed && result.diagnostics.size() == 4);
    require(result.diagnostics[0].code == DiagnosticCode::parser_syntax);
    require(result.diagnostics[1].code == DiagnosticCode::missing_terminator);
    require(result.diagnostics[2].code == DiagnosticCode::parser_contract);
    require(result.module.forms().size() == 1 && result.module.expression_count() == 1);
    require(render(result.diagnostics.back()) == "expected an Erlang form");
}

// Unexpected syntax-only events cannot disappear as successful ordinary forms.
void event_contract() {
    SourceManager sources;
    DirectiveReader reader(sources.add("directive.erl", "-define(X, 1)."));
    ParserSession parser;
    parser.consume(*reader.next());
    auto input = scan("f() -> ok.");
    parser.consume(OrdinaryForm{input});
    const auto result = std::move(parser).finish();
    require(result.failed && result.module.forms().empty() && result.diagnostics.size() == 2);
    require(result.diagnostics.front().code == DiagnosticCode::parser_contract);
}

// Feature tests inspect immutable snapshots rather than reconstructing removed directives.
bool enabled(const FeatureSnapshot &snapshot, std::string_view feature) {
    require(static_cast<bool>(snapshot));
    return std::ranges::find(snapshot->enabled, feature) != snapshot->enabled.end();
}

// Source directives and includes share live feature state but never mutate earlier snapshots.
void features() {
    PreprocessorOptions options;
    options.features = {{"maybe_expr", false}};
    options.read_file = [](const auto &) {
        return std::optional<std::string>("-feature(compr_assign, enable).\n-define(V, maybe).\n");
    };
    const auto result = parse("-module(m).\n-include(\"feature.hrl\").\nf() -> ?V.\n", options);
    require(result.succeeded());
    require(!enabled(result.module.features(), "maybe_expr"));
    require(enabled(result.module.features(), "compr_assign"));
    require(!enabled(result.module.features(result.module.forms().front()), "compr_assign"));
    require(enabled(result.module.features(result.module.forms().back()), "compr_assign"));
    require(std::get<ast::Atom>(body(result.module, U"f").value).name == U"maybe");
    const auto other = parse("-module(other).\nf() -> ok.\n");
    require(enabled(other.module.features(), "maybe_expr"));
    require(!enabled(other.module.features(), "compr_assign"));
}

// Verify logical mapping is applied once and errors preserve related macro/include origins.
void provenance() {
    PreprocessorOptions options;
    options.read_file = [](const auto &) { return std::optional<std::string>("-define(V, 42).\n"); };
    const auto result = parse("-include(\"v.hrl\").\n-file(\"logical.erl\", 40).\nf() -> ?V.\n", options);
    require(result.succeeded());
    const auto &expression = body(result.module, U"f");
    const auto &origin = result.module.anchor(expression.source);
    require(origin.location.file == "logical.erl");
    require(!origin.related.empty() && origin.spelling.source->name != "main.erl");
    options.read_file = [](const auto &) { return std::optional<std::string>("-define(V, #{bad =>}).\n"); };
    const auto error = parse("-include(\"v.hrl\").\nf() -> ?V.\ng() -> ok.\n", options);
    require(error.failed && !error.diagnostics[0].related.empty());
    require(error.diagnostics[0].location->file == "main.erl");
    require(function(error.module, U"g").clauses.front().body.size() == 1);
}

// Interleave two module sessions to detect accidental global parser/preprocessor state.
void interleaved() {
    SourceManager sources;
    PreprocessorSession a(sources.add("a.erl", "-define(V, 1).\nf() -> ?V."));
    PreprocessorSession b(sources.add("b.erl", "-define(V, 2).\nf() -> ?V."));
    ParserSession pa;
    ParserSession pb;
    while (true) {
        const auto ea = a.next();
        const auto eb = b.next();
        if (!ea && !eb) {
            break;
        }
        if (ea) {
            pa.consume(*ea);
        }
        if (eb) {
            pb.consume(*eb);
        }
    }
    const auto ra = std::move(pa).finish(a.features());
    const auto rb = std::move(pb).finish(b.features());
    require(ra.succeeded() && rb.succeeded());
    require(std::get<ast::IntegerLiteral>(body(ra.module, U"f").value).value.decimal == "1");
    require(std::get<ast::IntegerLiteral>(body(rb.module, U"f").value).value.decimal == "2");
}

// Token/node/diagnostic exhaustion is bounded, explicit, and never a successful partial module.
void limits() {
    ParserLimits limits;
    limits.form_tokens = 1;
    const auto tokens = parse("f() -> 1.", {}, limits);
    require(tokens.failed && tokens.diagnostics.front().code == DiagnosticCode::resource_limit);
    limits = {};
    limits.nodes = 3;
    const auto nodes = parse("f() -> {1}.", {}, limits);
    require(nodes.failed && nodes.module.expression_count() == 0);
    require(nodes.module.forms().size() == 1); // The initial implicit file attribute survives.
    limits = {};
    limits.total_tokens = 0;
    require(parse("f() -> 1.", {}, limits).failed);
    limits = {};
    limits.diagnostics = 1;
    const auto diagnostics = parse("bad() -> .\nbad() -> .\ngood() -> 1.", {}, limits);
    require(diagnostics.failed && diagnostics.diagnostics.size() == 2);
    require(diagnostics.diagnostics.back().code == DiagnosticCode::resource_limit);
    require(diagnostics.module.expression_count() == 0);
}

// Exercise the minimal native grammar, integration contracts, and recovery invariants.
int main() {
    scalars();
    recovery();
    raw_forms();
    event_contract();
    features();
    provenance();
    interleaved();
    limits();
}
