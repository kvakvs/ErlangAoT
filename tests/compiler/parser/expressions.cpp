// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
#include <clause/compiler/parser.hpp>
#include <stdexcept>

using namespace clause;

// Keep structural and provenance checks active in every build configuration.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("expression parser check failed");
    }
}

// Destroy source/session owners before inspecting the returned syntax tree.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("expr.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Flat storage survives large aggregates; excessive recursive syntax rolls back cleanly.
void bounds() {
    ParserLimits limits;
    limits.nesting = 8;
    require(parse("f() -> ((((((ok)))))).", limits).succeeded());
    const auto nested = parse("f() -> " + std::string(10000, '(') + "ok" + std::string(10000, ')') + ".", limits);
    require(nested.failed && nested.module.expression_count() == 0);
    require(nested.diagnostics.front().code == DiagnosticCode::resource_limit);
}

// Left-associative chains use iteration; recursive right/unary chains respect nesting limits.
void operator_bounds() {
    ParserLimits limits;
    limits.nesting = 8;
    const auto right = parse("f() -> A = B = C = D = E = F = G = H = I.", limits);
    require(right.failed && right.module.expression_count() == 0);
    const auto unary = parse("f() -> not not not not not not not not A.", limits);
    require(unary.failed && unary.diagnostics.front().code == DiagnosticCode::resource_limit);
}

int main() {
    bounds();
    operator_bounds();
}
