#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Keep contract checks active across optimized and sanitizer builds.
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("control syntax check failed");
}

// Destroy preprocessing owners before inspecting the returned syntax and provenance.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("control.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Return a committed function body after normal parsing or failed-form recovery.
const ast::Expression &body(const ParseResult &result) {
    const auto &function = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(function.clauses.front().body.front());
}

// Macro-generated branching retains candidate categories, ranges, and guard grouping.
void structure() {
    const auto result = parse("-define(C(X), case X of f() when true; false -> begin a,b end end).\nf() -> ?C(x).");
    require(result.succeeded());
    const auto &value = std::get<ast::CaseExpression>(body(result).value);
    const auto &branch = value.clauses.front();
    require(std::holds_alternative<ast::PatternCandidate>(result.module.pattern(branch.pattern).value));
    require(branch.guard && branch.guard->alternatives.size() == 2);
    require(!result.module.anchor(branch.source).related.empty());
    require(std::get<ast::BlockExpression>(result.module.expression(branch.body.front()).value).body.size() == 2);
    std::ostringstream printed;
    print_ast(printed, result.module);
    require(printed.str().find("PatternCandidate") != std::string::npos);
}

// Failed blocks rollback their patterns and expressions without swallowing the next form.
void recovery_and_limits() {
    auto result = parse("bad() -> case x of X -> begin end end. good() -> receive after 0 -> ok end.");
    require(result.failed && result.module.pattern_count() == 0);
    const auto &receive = std::get<ast::ReceiveExpression>(body(result).value);
    require(receive.clauses.empty() && receive.after);
    ParserLimits limits;
    limits.nesting = 8;
    std::string text = "f() -> ";
    for (int i = 0; i < 100; ++i)
        text += "begin ";
    text += "ok ";
    for (int i = 0; i < 100; ++i)
        text += "end ";
    auto exhausted = parse(text + '.', limits);
    require(exhausted.failed && exhausted.module.expression_count() == 0);
    require(exhausted.diagnostics.front().code == DiagnosticCode::resource_limit);
}

// Publicly published control nodes cannot contain empty mandatory bodies.
void invariants() {
    ast::Builder builder;
    Token end{};
    auto transaction = builder.begin({}, end);
    const auto source = builder.source(0, 0, 0);
    try {
        (void)builder.expression(ast::BlockExpression{}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)builder.expression(ast::ReceiveExpression{}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
}

int main() {
    structure();
    recovery_and_limits();
    invariants();
}
