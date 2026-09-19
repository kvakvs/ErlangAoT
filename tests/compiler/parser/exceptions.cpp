#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Keep checks active independently of assert/NDEBUG.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("fun/exception/maybe check failed");
    }
}

// Return owned syntax after preprocessing and source owners are destroyed.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("exceptions.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Inspect the final complete function, including recovered modules.
const ast::Expression &body(const ParseResult &result) {
    const auto &function = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(function.clauses.front().body.front());
}

// Fun names/arity remain typed, and named recursive clauses share checked head shapes.
void funs() {
    auto result = parse("f() -> {fun f/9999999999999999999999, fun M:F/A, fun Loop(0)->ok; Loop(N)->Loop(N-1) end}.");
    require(result.succeeded());
    const auto &tuple = std::get<ast::Tuple>(body(result).value);
    const auto &local = std::get<ast::LocalFunReference>(result.module.expression(tuple.elements[0]).value);
    require(local.arity.decimal == "9999999999999999999999");
    const auto &remote = std::get<ast::RemoteFunReference>(result.module.expression(tuple.elements[1]).value);
    require(std::holds_alternative<ast::Variable>(remote.arity));
    const auto &fun = std::get<ast::FunExpression>(result.module.expression(tuple.elements[2]).value);
    require(fun.name && fun.name->name == U"Loop" && fun.clauses.size() == 2);
    std::ostringstream output;
    print_ast(output, result.module);
    require(output.str().find("LocalFunReference") != std::string::npos);
    require(output.str().find("RemoteFunReference") != std::string::npos);
}

// Catch omission is distinct from explicit throw/_ and all reason roots are restricted.
void catches() {
    auto result = parse("-define(T, try f() catch R -> R; error:E:S when true -> {E,S} after done end). f() -> ?T.");
    require(result.succeeded());
    const auto &value = std::get<ast::TryExpression>(body(result).value);
    require(!value.of && value.handlers && value.after);
    const auto &first = value.handlers->front();
    require(!first.exception_class && !first.stacktrace);
    require(std::holds_alternative<ast::RestrictedPattern>(result.module.pattern(first.reason).value));
    require(!result.module.anchor(first.source).related.empty());
    const auto &second = value.handlers->back();
    require(second.exception_class && second.stacktrace && second.guard);
}

// Conditional candidates retain their operator anchor and feature state without re-lexing.
void maybe_features() {
    auto result = parse("f() -> maybe f() ?= value, done else _ -> no end.");
    require(result.succeeded());
    const auto &value = std::get<ast::MaybeExpression>(body(result).value);
    require(value.body.size() == 2 && value.otherwise);
    const auto &match = std::get<ast::MaybeMatch>(value.body.front());
    require(std::holds_alternative<ast::PatternCandidate>(result.module.pattern(match.pattern).value));
    const auto &anchor = result.module.anchor(match.source).spelling;
    require(anchor.source->spelling(anchor.begin, anchor.end) == "?=");
    auto disabled = parse("-feature(maybe_expr, disable). maybe() -> {maybe,else}.");
    require(disabled.succeeded());
    require(disabled.module.features()->enabled.empty());
    std::ostringstream output;
    print_ast(output, result.module);
    require(output.str().find("MaybeMatch") != std::string::npos);
}

// Incomplete funs and exceptions rollback and nesting limits apply through new constructs.
void failures() {
    auto result = parse("bad() -> fun A(X)->X; B(X)->X end. good() -> ok.");
    require(result.failed && result.module.pattern_count() == 0);
    require(std::holds_alternative<ast::Atom>(body(result).value));
    ParserLimits limits;
    limits.nesting = 4;
    auto nested = parse("f() -> try maybe fun () -> begin maybe ok end end end end after done end.", limits);
    require(nested.failed && nested.module.expression_count() == 0);
    require(nested.diagnostics.front().code == DiagnosticCode::resource_limit);
    ast::Builder builder;
    Token end{};
    auto transaction = builder.begin({}, end);
    const auto source = builder.source(0, 0, 0);
    try {
        (void)builder.expression(ast::FunExpression{}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)builder.expression(ast::MaybeExpression{}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
}

int main() {
    funs();
    catches();
    maybe_features();
    failures();
}
