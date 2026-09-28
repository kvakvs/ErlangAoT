// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
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

// Incomplete funs and exceptions rollback and nesting limits apply through new constructs.
void failures() {
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

int main() { failures(); }
