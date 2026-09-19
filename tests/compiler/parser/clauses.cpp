#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>
#include <type_traits>

using namespace erlang_aot;
static_assert(!std::is_convertible_v<ast::ExprId, ast::PatternSyntaxId>);

// Keep syntax, ownership and provenance checks active outside assertion builds.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("function clause check failed");
    }
}

// Check construction contracts independently of user syntax diagnostics.
template <typename Action> void rejects(Action action) {
    try {
        action();
    } catch (const std::invalid_argument &) {
        return;
    }
    throw std::runtime_error("expected invalid AST construction");
}

// Retain a completed module after all preprocessing owners have exited.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("clauses.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Retrieve the last successfully published function after recovery or implicit file forms.
const ast::Function &function(const ast::Module &module) {
    return std::get<ast::Function>(module.form(module.forms().back()).value);
}

// Preserve ordered argument patterns, macro origins, guard alternatives and body sequences.
void structure() {
    const auto result = parse("-define(P, {tag, X}).\nf(?P) when is_integer(X), X > 0; X == fallback -> A = X, A;\n"
                              "f(_) -> empty.\n");
    require(result.succeeded());
    const auto &f = function(result.module);
    require(f.name.name == U"f" && f.clauses.size() == 2);
    const auto &first = f.clauses[0];
    require(first.arguments.size() == 1 && first.body.size() == 2 && first.guard.has_value());
    require(first.guard->alternatives.size() == 2);
    require(first.guard->alternatives[0].tests.size() == 2);
    require(first.guard->alternatives[1].tests.size() == 1);
    require(result.module.anchor(first.guard->source).location.line == 2);
    const auto &pattern = result.module.pattern(first.arguments[0]);
    const auto &restricted = std::get<ast::RestrictedPattern>(pattern.value);
    require(std::holds_alternative<ast::Tuple>(result.module.expression(restricted.expression).value));
    require(!result.module.anchor(pattern.source).related.empty());
    require(result.module.anchor(first.source).location.line == 2);
    require(result.module.anchor(f.clauses[1].source).location.line == 3);
    require(!f.clauses[1].guard && f.clauses[1].body.size() == 1);
}

// Restricted roots and parentheses differ from OTP's expression-based nested containers.
void acceptance() {
    require(parse("f({g(), A ! B}, [M:F|h()], (A = B = C)) when g(), A = B; catch h() -> Unbound.").succeeded());
    require(parse("f() -> first.\nf() -> second.").succeeded());
    for (const std::string head : {"g()", "(g())", "A ! B", "catch A", "A andalso B", "M:F"}) {
        const auto result = parse("bad(" + head + ") -> bad.\ngood(X) -> X.");
        require(result.failed && function(result.module).name.name == U"good");
        require(result.module.pattern_count() == 1 && result.module.expression_count() == 2);
    }
    const auto mismatch = parse("f(X) -> X;\ng(X) -> X.\ngood() -> ok.");
    require(mismatch.failed && mismatch.diagnostics.front().code == DiagnosticCode::parser_syntax);
    require(mismatch.diagnostics.front().location->line == 2);
    require(mismatch.module.pattern_count() == 0 && mismatch.module.expression_count() == 1);
}

// Pattern arena allocations participate in node limits and all-or-nothing form rollback.
void limits() {
    ParserLimits limits;
    limits.nodes = 4;
    const auto failed = parse("f(X) -> X.", limits);
    require(failed.failed && failed.module.pattern_count() == 0 && failed.module.expression_count() == 0);
    require(failed.diagnostics.front().code == DiagnosticCode::resource_limit);
    limits.nodes = 5;
    const auto success = parse("f(X) -> X.", limits);
    require(success.succeeded() && success.module.pattern_count() == 1);
    limits.nodes = 8;
    const auto second = parse("f(X) -> X.\ng(Y) -> Y.", limits);
    require(second.failed && second.module.pattern_count() == 1 && function(second.module).name.name == U"f");
}

// Pattern categories remain explicit when visiting; candidates never claim pat_expr validation.
struct PatternKind {
    bool operator()(const ast::RestrictedPattern &) const { return false; }

    bool operator()(const ast::PatternCandidate &) const { return true; }
};

// Pattern IDs reject stale and foreign owners, while owner moves preserve identity.
void pattern_ownership() {
    ast::Builder builder;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic", 1, 1}, {}};
    std::optional<ast::PatternSyntaxId> stale;
    {
        auto tx = builder.begin({}, eof);
        const auto source = builder.source(0, 0, 0);
        const auto atom = builder.expression(ast::Atom{U"f"}, source);
        stale = builder.pattern(ast::RestrictedPattern{atom}, source);
    }
    auto tx = builder.begin({}, eof);
    const auto source = builder.source(0, 0, 0);
    const auto atom = builder.expression(ast::Atom{U"f"}, source);
    const auto call = builder.expression(ast::CallExpression{atom, {}}, source);
    const auto candidate = builder.pattern(ast::PatternCandidate{call}, source);
    rejects([&] { (void)builder.view().pattern(*stale); });
    require(builder.view().visit(candidate, PatternKind{}));
    rejects([&] { builder.form(ast::Function{{U"f"}, {{{candidate}, {}, {atom}, source}}}, source); });
    ast::Builder foreign;
    auto other = foreign.begin({}, eof);
    rejects([&] { foreign.pattern(ast::PatternCandidate{call}, foreign.source(0, 0, 0)); });
    rejects([&] { (void)foreign.view().pattern(candidate); });
    tx.commit(builder.form(ast::ModuleAttribute{{U"m"}}, source));
    auto module = std::move(builder).finish();
    ast::Module moved(std::move(module));
    require(moved.pattern_count() == 1 && moved.visit(candidate, PatternKind{}));
    auto next = foreign.expression(ast::Atom{U"other"}, foreign.source(0, 0, 0));
    require(next != atom);
}

// Reject empty clauses, empty guards/conjunctions/bodies, and inconsistent embedded arities.
void invariants() {
    ast::Builder builder;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic", 1, 1}, {}};
    auto tx = builder.begin({}, eof);
    const auto source = builder.source(0, 0, 0);
    const auto atom = builder.expression(ast::Atom{U"ok"}, source);
    const auto pattern = builder.pattern(ast::RestrictedPattern{atom}, source);
    rejects([&] { builder.form(ast::Function{{U"f"}, {}}, source); });
    rejects([&] { builder.form(ast::Function{{U"f"}, {{{}, {}, {}, source}}}, source); });
    ast::FunctionClause clause{{}, ast::GuardSyntax{{}, source}, {atom}, source};
    rejects([&] { builder.form(ast::Function{{U"f"}, {clause}}, source); });
    clause.guard->alternatives.push_back({{}, source});
    rejects([&] { builder.form(ast::Function{{U"f"}, {clause}}, source); });
    clause.guard->alternatives[0].tests.push_back(atom);
    auto second = clause;
    second.arguments.push_back(pattern);
    rejects([&] { builder.form(ast::Function{{U"f"}, {clause, second}}, source); });
    tx.commit(builder.form(ast::Function{{U"f"}, {clause}}, source));
}

int main() {
    structure();
    acceptance();
    limits();
    pattern_ownership();
    invariants();
}
