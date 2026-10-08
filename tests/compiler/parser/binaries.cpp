// Source syntax and recovery goldens live in frontend_cli; this suite retains API-only invariants.
#include "ast/builder.hpp"
#include <clause/compiler/parser.hpp>
#include <stdexcept>

using namespace clause;

// Check syntax structure and metadata independently of runtime bitstring evaluation.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("binary syntax check failed");
    }
}

// Returned ASTs must retain values and origins after preprocessing owners leave scope.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("binaries.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Nested expr_max calls obey the shared depth bound; segment/modifier spines iterate.
void limits() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string nested = "f() -> ";
    for (int i = 0; i < 1000; ++i) {
        nested += "<<";
    }
    nested += "1";
    for (int i = 0; i < 1000; ++i) {
        nested += ">>";
    }
    const auto error = parse(nested + ".", limits);
    require(error.failed && error.module.expression_count() == 0);
    require(error.diagnostics.front().code == DiagnosticCode::resource_limit);
    limits.nodes = 2;
    const auto sigil = parse("f() -> ~b\"x\".", limits);
    require(sigil.failed && sigil.module.expression_count() == 0);
}

// Segment sizes and modifier spans must belong to the active form, with nonempty explicit types.
void invariants() {
    ast::Builder builder;
    ast::Builder foreign;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic", 1, 1}, {}};
    auto a = builder.begin({}, eof);
    auto b = foreign.begin({}, eof);
    const auto source = builder.source(0, 0, 0);
    const auto other = foreign.source(0, 0, 0);
    const auto value = builder.expression(ast::Atom{U"v"}, source);
    const auto size = foreign.expression(ast::Atom{U"n"}, other);
    std::vector<ast::BinarySegment> invalid{
        {value, {}, std::vector<ast::BinaryModifier>{}, source},
        {value, size, {}, source},
        {value, {}, std::vector<ast::BinaryModifier>{{{U"unit"}, Integer{"8"}, other}}, source}};
    for (const auto &segment : invalid) {
        bool rejected = false;
        try {
            builder.expression(ast::Bitstring{{segment}}, source);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected);
    }
}

int main() {
    limits();
    invariants();
}
