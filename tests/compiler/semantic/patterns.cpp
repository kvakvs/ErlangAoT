#include "ast/builder.hpp"
#include "semantic/binding_state.hpp"
#include "semantic/match_plan.hpp"
#include "semantic/services.hpp"
#include <algorithm>
#include <cstdio>
#include <erlang_aot/compiler/parser.hpp>
#include <source_location>
#include <stdexcept>

using namespace erlang_aot;
using namespace erlang_aot::semantic;

// Source cannot reach private plan ceilings cheaply; verify transaction failure with a deliberately tiny budget.
void match_budget() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("match.erl", "-module(match). f(A=B,A) -> B."));
    auto parsed = parse_module(pp);
    auto module = index(parsed.module, "match.erl", [](const Diagnostic &) { throw std::runtime_error("index"); });
    bind_parameters(*module, [](const Diagnostic &) { throw std::runtime_error("bindings"); });
    std::vector<Diagnostic> errors;
    const auto reporter = [&](const Diagnostic &d) { errors.push_back(d); };
    const auto &function = module->functions.front();
    if (make_match_plan(*module, function, reporter, {.work_limit = 2}) || errors.size() != 1) {
        throw std::runtime_error("partial plan escaped budget");
    }
    errors.clear();
    const auto plan = make_match_plan(*module, function, reporter);
    if (!plan || !errors.empty() || plan->outputs.size() != 2 || plan->nodes.size() != 5) {
        throw std::runtime_error("plan retry failed");
    }
}

// Flat normalization and transactional side tables are not observable through matching until step 6.
void require(bool condition, const std::source_location site = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error("pattern invariant at line " + std::to_string(site.line()));
    }
}

// Use parsed real source for normalization while keeping capability rejection independent.
void normalization() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("patterns.erl", R"(
-module(patterns).
f((Whole) = {1+2*3, $a, 1/2, -0.0, X}) -> Whole.
g(V) -> (A = B) = V, B.
)"));
    auto parsed = parse_module(pp);
    require(!parsed.failed);
    auto module = index(parsed.module, "patterns.erl", [](const Diagnostic &) { require(false); });
    bind_parameters(*module, [](const Diagnostic &) { require(false); });
    const auto &nodes = module->functions[0].patterns;
    require(nodes.front().kind == PatternKind::alias);
    require(nodes[1].origin != nodes[1].expression && nodes[1].kind == PatternKind::variable);
    require(nodes[2].kind == PatternKind::tuple && nodes[2].children.size() == 5);
    require(std::get<ast::IntegerLiteral>(*nodes[3].literal).value.decimal == "7");
    require(std::get<ast::IntegerLiteral>(*nodes[4].literal).value.decimal == "97");
    require(std::get<ast::FloatLiteral>(*nodes[5].literal).value == 0.5);
    const auto &body = module->functions[1].patterns;
    require(std::ranges::count(body, PatternKind::alias, &NormalizedPattern::kind) == 1);
    require(module->functions[1].clause_bindings[0].definitions.size() == 3);
    for (const auto &node : nodes) {
        require(parsed.module.anchor(parsed.module.expression(node.origin).source).location.file == "patterns.erl");
    }
    // Both ordinary semantic failure and resource failure must discard earlier function results.
    std::vector<Diagnostic> errors;
    bind_parameters(*module, [&](const Diagnostic &d) { errors.push_back(d); }, 8);
    require(errors.size() == 1 && errors[0].message == "binding analysis work limit exceeded");
    for (const auto &function : module->functions) {
        require(function.patterns.empty() && function.bindings.empty() && function.clause_bindings.empty());
    }
}

// Candidate syntax shares exactly the restricted-pattern validator, including at depths beyond parser admission.
void candidates(bool permissive, bool illegal) {
    ast::Builder builder;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"candidate", 1, 1}, {}};
    std::optional<ast::PatternSyntaxId> root;
    {
        auto transaction = builder.begin({}, eof);
        const auto source = builder.source(0, 0, 0);
        auto value = builder.expression(ast::IntegerLiteral{{"1"}}, source);
        if (illegal) {
            const auto target = builder.expression(ast::Atom{U"id"}, source);
            value = builder.expression(ast::CallExpression{target, {value}}, source);
        }
        for (unsigned i = 0; i < 12000; ++i) {
            value = builder.expression(ast::Tuple{{value}}, source);
        }
        ast::PatternValue pattern = ast::RestrictedPattern{value};
        if (permissive) {
            pattern = ast::PatternCandidate{value};
        }
        root = builder.pattern(std::move(pattern), source);
        transaction.commit(builder.form(ast::ModuleAttribute{{U"candidate"}}, source));
    }
    auto syntax = std::move(builder).finish();
    std::vector<Diagnostic> errors;
    const Reporter report = [&](const Diagnostic &d) { errors.push_back(d); };
    auto module = index(syntax, "candidate", report);
    Function function{{U"f", 1}, syntax.forms().front(), false, ""};
    function.clause_bindings.resize(1);
    std::size_t work = 0;
    BindingAnalysis state{*module, function, report, 0, work, 100000};
    BindingEnvironment incoming;
    BindingCandidate candidate{incoming, {}};
    bind_pattern(state, *root, candidate, BindingContext::head, 0);
    require(candidate.valid != illegal);
    require(errors.size() == static_cast<std::size_t>(illegal));
    if (illegal) {
        require(errors[0].message == "illegal pattern");
    } else {
        require(function.patterns.size() == 12001);
    }
}

// A semantic error after a completed good function cannot leave a partially reusable normalized module.
void rollback() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("rollback.erl", "-module(rollback). good(X) -> X. bad({length([])}) -> ok."));
    auto parsed = parse_module(pp);
    require(!parsed.failed);
    std::vector<Diagnostic> errors;
    const Reporter report = [&](const Diagnostic &d) { errors.push_back(d); };
    auto module = index(parsed.module, "rollback.erl", report);
    bind_parameters(*module, report);
    require(errors.size() == 1 && errors.front().message == "illegal pattern");
    for (const auto &function : module->functions) {
        require(function.patterns.empty() && function.bindings.empty() && function.clause_bindings.empty());
    }
}

// Deliberate private exhaustion must discard service identities across the module and permit reanalysis.
void service_budget() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("services.erl", "-module(services). f(X) when is_integer(X) -> is_atom(X)."));
    auto parsed = parse_module(pp);
    std::vector<Diagnostic> errors;
    const Reporter report = [&](const Diagnostic &d) { errors.push_back(d); };
    auto module = index(parsed.module, "services.erl", report);
    bind_parameters(*module, report);
    resolve_services(*module, report, 1);
    require(!errors.empty() && module->functions.front().services.empty());
    errors.clear();
    resolve_services(*module, report);
    require(errors.empty() && module->functions.front().services.size() == 2);
}

int main() {
    try {
        match_budget();
        service_budget();
        normalization();
        rollback();
        for (const bool permissive : {false, true}) {
            candidates(permissive, false);
            candidates(permissive, true);
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
