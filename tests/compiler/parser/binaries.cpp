#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Check syntax structure and metadata independently of runtime bitstring evaluation.
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("binary syntax check failed");
}

// Returned ASTs must retain values and origins after preprocessing owners leave scope.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("binaries.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Find the final committed function's first body expression after possible recovery.
const ast::Expression &body(const ParseResult &result) {
    const auto &function = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(function.clauses.front().body.front());
}

// Preserve omitted/explicit defaults, numeric precision, modifier order and exact token spans.
void segments() {
    const auto result = parse("f() -> <<X, Y:default/default, Z:N/custom-unit:999999999999999999999999999-custom>>.");
    require(result.succeeded());
    const auto &segments = std::get<ast::Bitstring>(body(result).value).segments;
    require(segments.size() == 3 && !segments[0].size && !segments[0].modifiers);
    require(segments[1].size && segments[1].modifiers && segments[1].modifiers->size() == 1);
    require(std::get<ast::Atom>(result.module.expression(*segments[1].size).value).name == U"default");
    require(segments[1].modifiers->front().name.name == U"default");
    const auto &modifiers = *segments[2].modifiers;
    require(modifiers.size() == 3 && modifiers.front().name.name == U"custom" &&
            modifiers.back().name.name == U"custom");
    require(modifiers[1].parameter->decimal == "999999999999999999999999999");
    require(result.module.extent(modifiers[1].source).size() == 3);
    require(result.module.extent(segments[2].source).size() == 11);
}

// Sigils use the ordinary segment representation while retaining all macro/source provenance.
void sigils() {
    const auto result = parse("-define(S, ~b\"λ\\n\").\nf() -> ?S.");
    require(result.succeeded());
    const auto &root = body(result);
    const auto &segment = std::get<ast::Bitstring>(root.value).segments.front();
    const auto &string = result.module.expression(segment.value);
    require(std::get<ast::StringLiteral>(string.value).value == U"λ\n");
    require(!segment.size && segment.modifiers && segment.modifiers->front().name.name == U"utf8");
    require(result.module.extent(root.source).size() == 3 && result.module.extent(string.source).size() == 1);
    require(result.module.extent(segment.modifiers->front().source).size() == 1);
    require(!result.module.anchor(segment.source).related.empty());
    require(!result.module.anchor(segment.modifiers->front().source).related.empty());
    require(result.module.anchor(segment.source).location.line == 2);
    const auto explicit_utf8 = parse("f() -> <<\"λ\\n\"/utf8>>.");
    const auto &ordinary = std::get<ast::Bitstring>(body(explicit_utf8).value).segments.front();
    require(std::get<ast::StringLiteral>(explicit_utf8.module.expression(ordinary.value).value).value == U"λ\n");
}

// Binary pattern contents use bit_expr, not a recursively restricted semantic pattern grammar.
void contexts() {
    const auto result = parse("f(<<(g()):N/unknown>>) -> <<-(A+B):(f())/integer>>.");
    require(result.succeeded());
    const auto &function = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    const auto &pattern =
        std::get<ast::RestrictedPattern>(result.module.pattern(function.clauses.front().arguments.front()).value);
    require(std::holds_alternative<ast::Bitstring>(result.module.expression(pattern.expression).value));
    for (const std::string text : {"<<X+Y>>", "<<X:-1>>", "<<X:f()>>", "<<- -1>>", "<<X/unit:-1>>"}) {
        const auto error = parse("bad(P) -> " + text + ".\ngood() -> <<>>.");
        require(error.failed && error.module.expression_count() == 1 && error.module.pattern_count() == 0);
    }
    require(parse("f() -> <<M:F>>.").succeeded()); // Colon denotes size, not a remote expression.
    const auto division = parse("f() -> <<(A/B):(N/2)/integer>>.");
    require(division.succeeded());
}

// Nested expr_max calls obey the shared depth bound; segment/modifier spines iterate.
void limits() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string nested = "f() -> ";
    for (int i = 0; i < 1000; ++i)
        nested += "<<";
    nested += "1";
    for (int i = 0; i < 1000; ++i)
        nested += ">>";
    const auto error = parse(nested + ".", limits);
    require(error.failed && error.module.expression_count() == 0);
    require(error.diagnostics.front().code == DiagnosticCode::resource_limit);
    const auto default_limit = parse(nested + ".");
    require(default_limit.failed && default_limit.diagnostics.front().code == DiagnosticCode::resource_limit);
    std::string flat = "f() -> <<1";
    for (int i = 0; i < 4096; ++i)
        flat += ",1";
    const auto many = parse(flat + ">>.", limits);
    require(many.succeeded() && std::get<ast::Bitstring>(body(many).value).segments.size() == 4097);
    std::string types = "f() -> <<1/integer";
    for (int i = 0; i < 4096; ++i)
        types += "-unknown";
    require(parse(types + ">>.", limits).succeeded());
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
    segments();
    sigils();
    contexts();
    limits();
    invariants();
}
