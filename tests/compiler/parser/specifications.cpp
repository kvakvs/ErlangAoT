#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <source_location>
#include <sstream>

using namespace erlang_aot;

// Retain precise assertion locations in optimized as well as debug test builds.
void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("specification check line " + std::to_string(location.line()));
}

// Parse expanded forms with short-lived sessions to exercise owned provenance.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("specifications.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Local/qualified names, products and constraint styles survive into immutable payloads.
void structure() {
    auto result = parse("-spec m:f(A) -> A when A :: integer(), is_subtype(A, 1..10); "
                        "(A,B) -> {A,B}. -callback run() -> ok. -spec any() -> ok; (...) -> any().");
    require(result.succeeded());
    const auto &module = result.module;
    const auto &spec = std::get<ast::Specification>(module.form(module.forms()[1]).value);
    require(spec.module->name == U"m" && spec.name.name == U"f" && spec.arity == 1);
    require(spec.signatures.size() == 2 && spec.signatures[1].function.arguments->size() == 2);
    require(!spec.signatures[0].constraints[0].legacy && spec.signatures[0].constraints[1].legacy);
    require(std::holds_alternative<ast::RangeType>(module.type(spec.signatures[0].constraints[1].bound).value));
    require(std::get<ast::Specification>(module.form(module.forms()[2]).value).callback);
    require(module.anchor(spec.signatures[0].source).spelling.source != nullptr);
    std::ostringstream tree;
    print_ast(tree, module);
    require(tree.str().find("Specification name=f arity=1 module=m") != std::string::npos);
    require(tree.str().find("Callback name=run arity=0") != std::string::npos);
}

// Rejected constraints and helper-builder inputs roll back types and preserve later forms.
void recovery() {
    const auto result = parse("-spec bad(A) -> A when A :: integer(), _ :: atom(). "
                              "-record broken() -> ok. -spec any(...) -> atom(). -spec good() -> ok.");
    require(result.failed && result.diagnostics.size() == 3);
    require(result.module.forms().size() == 2 && result.module.type_count() == 1);
    ParserLimits limits;
    limits.nodes = 7;
    const auto bounded = parse("-spec f(A,B,C) -> {A,B,C}.", limits);
    require(bounded.failed && bounded.module.type_count() == 0);
}

// Builders enforce first-product structure while leaving overload semantic consistency alone.
void builder_checks() {
    ast::Builder builder;
    auto tx = builder.begin({}, Token{});
    const auto source = builder.source(0, 0, 0);
    const auto result = builder.type(ast::Atom{U"ok"}, source);
    ast::Specification spec{false, {}, {U"f"}, 0, {{{{}, result}, {}, source}}};
    bool rejected = false;
    try {
        builder.form(spec, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    spec.signatures[0].function.arguments.emplace();
    tx.commit(builder.form(std::move(spec), source));
}

int main() {
    structure();
    recovery();
    builder_checks();
}
