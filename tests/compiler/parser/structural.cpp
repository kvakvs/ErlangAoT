// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Preserve checks in every build mode, including provenance and syntax categories.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("structural expression check failed");
    }
}

// Run the real frontend and inspect its AST after source/session destruction.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("structural.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Structural postfix chains iterate, while nested values retain the configured depth bound.
void limits_and_recovery() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string nested = "f() -> ";
    for (int i = 0; i < 100; ++i) {
        nested += "#{a => ";
    }
    nested += "ok" + std::string(100, '}') + ".";
    const auto exhausted = parse(nested, limits);
    require(exhausted.failed && exhausted.module.expression_count() == 0);
    require(exhausted.diagnostics.front().code == DiagnosticCode::resource_limit);
}

// Child and field sources cannot cross form/owner boundaries during AST construction.
void invariants() {
    ast::Builder a;
    ast::Builder b;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic", 1, 1}, {}};
    auto first = a.begin({}, eof);
    auto second = b.begin({}, eof);
    const auto source = a.source(0, 0, 0);
    const auto other = b.source(0, 0, 0);
    const auto atom = a.expression(ast::Atom{U"ok"}, source);
    bool rejected = false;
    try {
        a.expression(ast::MapExpression{{}, {{ast::MapFieldKind::exact, atom, atom, other}}}, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    rejected = false;
    try {
        b.expression(ast::RecordExpression{{}, {ast::InferredRecordName{}, other}, {{{ast::Atom{U"a"}}, atom, other}}},
                     other);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
}

int main() {
    limits_and_recovery();
    invariants();
}
