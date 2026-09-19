#include "ast/builder.hpp"
#include <algorithm>
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Keep native contracts checked in Debug, optimized, and sanitizer builds.
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("comprehension check failed");
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

// Zip grouping and arrow strictness survive macros independently of candidate legality.
void qualifiers() {
    auto result = parse("-define(G, X <:- L). f() -> [X,X+1 || ?G && <<Y>> <= B, Z = X].");
    require(result.succeeded());
    const auto &value = std::get<ast::ListComprehension>(body(result).value);
    require(value.templates.size() == 2 && value.qualifiers.size() == 2);
    const auto &zip = std::get<ast::ZippedQualifier>(value.qualifiers.front());
    require(zip.qualifiers.size() == 2);
    const auto &list = std::get<ast::ListGenerator>(zip.qualifiers[0].value);
    const auto &binary = std::get<ast::BinaryGenerator>(zip.qualifiers[1].value);
    require(list.strict && !binary.strict);
    require(std::holds_alternative<ast::PatternCandidate>(result.module.pattern(list.pattern).value));
    require(!result.module.anchor(zip.qualifiers[0].source).related.empty());
    const auto &filter = std::get<ast::FilterQualifier>(std::get<ast::Qualifier>(value.qualifiers.back()).value);
    require(std::holds_alternative<ast::MatchExpression>(result.module.expression(filter.expression).value));
    std::ostringstream output;
    print_ast(output, result.module);
    require(output.str().find("ZippedQualifier qualifiers=2") != std::string::npos);
}

// Map templates retain association/exact syntax, and binary templates retain expr_max grouping.
void templates() {
    auto result = parse("f() -> {#{K=>V,K:=V || K:=V <:- M}, << (f(X)) || X<-L >>}.");
    require(result.succeeded());
    const auto &tuple = std::get<ast::Tuple>(body(result).value);
    const auto &map = std::get<ast::MapComprehension>(result.module.expression(tuple.elements[0]).value);
    require(map.templates.size() == 2 && map.templates[1].kind == ast::MapFieldKind::exact);
    const auto &generator = std::get<ast::MapGenerator>(std::get<ast::Qualifier>(map.qualifiers.front()).value);
    require(generator.strict);
    const auto &binary = std::get<ast::BinaryComprehension>(result.module.expression(tuple.elements[1]).value);
    require(std::holds_alternative<ast::Group>(result.module.expression(binary.expression).value));
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
    std::string flat = "f() -> [x || true";
    for (int i = 0; i < 4096; ++i)
        flat += ",true";
    require(parse(flat + "].", limits).succeeded());
    std::string nested = "f() -> ";
    for (int i = 0; i < 100; ++i)
        nested += "[X || X <- ";
    auto exhausted = parse(nested + "[]" + std::string(100, ']') + '.', limits);
    require(exhausted.failed && exhausted.module.expression_count() == 0);
    require(exhausted.diagnostics.front().code == DiagnosticCode::resource_limit);
    auto recovery = parse("bad() -> [X || X<-L,]. good() -> ok.");
    require(recovery.failed && recovery.module.pattern_count() == 0);
    require(std::holds_alternative<ast::Atom>(body(recovery).value));
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
    qualifiers();
    templates();
    features();
    limits_and_recovery();
    invariants();
}
