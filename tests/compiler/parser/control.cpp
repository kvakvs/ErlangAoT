// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Keep contract checks active across optimized and sanitizer builds.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("control syntax check failed");
    }
}

// Destroy preprocessing owners before inspecting the returned syntax and provenance.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("control.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Failed blocks rollback their patterns and expressions without swallowing the next form.
void recovery_and_limits() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string text = "f() -> ";
    for (int i = 0; i < 100; ++i) {
        text += "begin ";
    }
    text += "ok ";
    for (int i = 0; i < 100; ++i) {
        text += "end ";
    }
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
    recovery_and_limits();
    invariants();
}
