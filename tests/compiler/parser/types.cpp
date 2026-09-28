#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <source_location>
#include <sstream>
#include <type_traits>

using namespace erlang_aot;
static_assert(!std::is_convertible_v<ast::ExprId, ast::TypeId>);
static_assert(!std::is_convertible_v<ast::TermId, ast::TypeId>);

// Keep type tests active with source-line diagnostics in all configurations.
void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        throw std::runtime_error("type check line " + std::to_string(location.line()));
    }
}

// Destroy source sessions before inspecting owned type nodes.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("types.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Injected node/depth exhaustion must roll back types; ordinary syntax is covered by frontend_cli.
void recovery() {
    ParserLimits limits;
    limits.nodes = 8;
    auto result = parse("-type t() :: {a,b,c,d,e,f}.", limits);
    require(result.failed && result.module.type_count() == 0);
    limits = {};
    limits.nesting = 8;
    result = parse("-type t() :: [[[[[[[[[atom()]]]]]]]]].", limits);
    require(result.failed && result.diagnostics[0].code == DiagnosticCode::resource_limit);
}

// Rollback generations and foreign owners are rejected at type-child construction.
void ownership() {
    ast::Builder builder;
    std::optional<ast::TypeId> stale;
    {
        auto tx = builder.begin({}, Token{});
        stale = builder.type(ast::Atom{U"old"}, builder.source(0, 0, 0));
    }
    auto tx = builder.begin({}, Token{});
    const auto source = builder.source(0, 0, 0);
    const auto fresh = builder.type(ast::Atom{U"new"}, source);
    bool rejected = false;
    try {
        builder.type(ast::TypeGroup{*stale}, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    ast::Builder foreign;
    auto other = foreign.begin({}, Token{});
    const auto other_id = foreign.type(ast::Atom{U"foreign"}, foreign.source(0, 0, 0));
    rejected = false;
    try {
        builder.type(ast::TypeGroup{other_id}, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    tx.commit(builder.form(ast::TypeDeclaration{ast::TypeDeclarationKind::alias, {U"t"}, {}, fresh}, source));
}

int main() {
    recovery();
    ownership();
}
