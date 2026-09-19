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
    if (!value)
        throw std::runtime_error("type check line " + std::to_string(location.line()));
}

// Destroy source sessions before inspecting owned type nodes.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("types.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Preserve declarations, parameters, annotations and mixed field categories.
void structure() {
    auto result = parse("-type (t(A)) :: X :: {A, [integer(),...], #{atom() := binary()}}. "
                        "-record #Point{field = run() :: integer(), plain}. "
                        "-nominal n(A) :: other:t(A).");
    require(result.succeeded());
    auto moved = std::move(result.module);
    const auto &declaration = std::get<ast::TypeDeclaration>(moved.form(moved.forms()[1]).value);
    require(declaration.parameters[0].name == U"A");
    require(std::holds_alternative<ast::AnnotatedType>(moved.type(declaration.type).value));
    const auto &record = std::get<ast::RecordDeclaration>(moved.form(moved.forms()[2]).value);
    require(record.fields[0].type && record.fields[0].default_value && !record.fields[1].type);
    require(moved.anchor(moved.type(declaration.type).source).spelling.source != nullptr);
    std::ostringstream tree;
    print_ast(tree, moved);
    require(tree.str().find("kind=nominal") != std::string::npos);
    require(tree.str().find("MapTypeField kind=:=") != std::string::npos);
}

// Syntax failures and resource exhaustion cannot publish partial type arenas.
void recovery() {
    auto result = parse("-type broken() :: {integer(),}. -type good() :: atom().");
    require(result.failed && result.module.type_count() == 1);
    require(result.module.forms().size() == 2);
    ParserLimits limits;
    limits.nodes = 8;
    result = parse("-type t() :: {a,b,c,d,e,f}.", limits);
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
    structure();
    recovery();
    ownership();
}
