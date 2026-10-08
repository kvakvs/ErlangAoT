#include "ast/builder.hpp"
#include <clause/compiler/parser.hpp>
#include <clause/compiler/printing.hpp>
#include <source_location>
#include <sstream>

using namespace clause;

// Retain precise assertion locations in optimized as well as debug test builds.
void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        throw std::runtime_error("specification check line " + std::to_string(location.line()));
    }
}

// Parse expanded forms with short-lived sessions to exercise owned provenance.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("specifications.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Tiny node budgets are API-only; malformed specifications now use frontend_cli goldens.
void recovery() {
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
    recovery();
    builder_checks();
}
