// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
#include "ast/builder.hpp"
#include <algorithm>
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Keep native contracts checked in Debug, optimized, and sanitizer builds.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("comprehension check failed");
    }
}

// Preserve AST/source ownership beyond the real preprocessing pipeline's lifetime.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("comprehensions.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Access the last committed function after either success or recovery.
const ast::Expression &body(const ParseResult &result) {
    const auto &function = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(function.clauses.front().body.front());
}

// Feature flags travel with forms even when both states accept the same match qualifier syntax.
void features() {
    for (const bool enabled : {false, true}) {
        const std::string setting = enabled ? "enable" : "disable";
        auto result = parse("-feature(compr_assign," + setting + "). f() -> [X || X=1].");
        require(result.succeeded());
        const auto snapshot = result.module.features(result.module.forms().back());
        require((std::ranges::find(snapshot->enabled, "compr_assign") != snapshot->enabled.end()) == enabled);
        require(std::holds_alternative<ast::ListComprehension>(body(result).value));
    }
}

// Flat qualifiers iterate, nested inputs consume depth, and invalid forms rollback all candidates.
void limits_and_recovery() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string nested = "f() -> ";
    for (int i = 0; i < 100; ++i) {
        nested += "[X || X <- ";
    }
    auto exhausted = parse(nested + "[]" + std::string(100, ']') + '.', limits);
    require(exhausted.failed && exhausted.module.expression_count() == 0);
    require(exhausted.diagnostics.front().code == DiagnosticCode::resource_limit);
}

// Constructors reject empty templates and invalid singleton zip groups.
void invariants() {
    ast::Builder builder;
    Token end{};
    auto transaction = builder.begin({}, end);
    const auto source = builder.source(0, 0, 0);
    try {
        (void)builder.expression(ast::ListComprehension{}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
    const auto id = builder.expression(ast::Atom{U"true"}, source);
    ast::Qualifier filter{ast::FilterQualifier{id}, source};
    try {
        (void)builder.expression(ast::ListComprehension{{id}, {ast::ZippedQualifier{{filter}, source}}}, source);
        require(false);
    } catch (const std::invalid_argument &) {
    }
}

int main() {
    features();
    limits_and_recovery();
    invariants();
}
