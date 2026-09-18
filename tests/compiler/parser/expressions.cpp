#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Keep structural and provenance checks active in every build configuration.
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("expression parser check failed");
}

// Destroy source/session owners before inspecting the returned syntax tree.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("expr.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Locate a completed function after the implicit file attribute.
const ast::Expression &body(const ParseResult &result) {
    const auto &function = std::get<ast::ZeroArgumentFunction>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(function.body.front());
}

// Preserve typed aggregates, grouping extents, wildcard spelling and macro origins.
void aggregates() {
    const auto result = parse("-define(V, {_, $λ}).\nf() -> [?V, (\"a\" \"b\")|Tail].\n");
    require(result.succeeded());
    const auto &root = body(result);
    const auto &list = std::get<ast::List>(root.value);
    require(list.elements.size() == 2 && list.tail.has_value());
    require(std::get<ast::Variable>(result.module.expression(*list.tail).value).name == U"Tail");
    const auto &tuple_node = result.module.expression(list.elements.front());
    const auto &tuple = std::get<ast::Tuple>(tuple_node.value);
    require(!result.module.anchor(tuple_node.source).related.empty());
    require(std::get<ast::Variable>(result.module.expression(tuple.elements[0]).value).name == U"_");
    require(std::get<ast::CharacterLiteral>(result.module.expression(tuple.elements[1]).value).value == U'λ');
    const auto &group_node = result.module.expression(list.elements[1]);
    require(result.module.extent(group_node.source).size() == 4);
    const auto &group = std::get<ast::Group>(group_node.value);
    const auto &string = result.module.expression(group.expression);
    require(std::get<ast::StringLiteral>(string.value).value == U"ab");
    require(result.module.extent(string.source).size() == 2);
    require(result.module.extent(root.source).size() == 14);
}

// Flat storage survives large aggregates; excessive recursive syntax rolls back cleanly.
void bounds() {
    ParserLimits limits;
    limits.nesting = 8;
    require(parse("f() -> ((((((ok)))))).", limits).succeeded());
    const auto nested = parse("f() -> " + std::string(10000, '(') + "ok" + std::string(10000, ')') + ".", limits);
    require(nested.failed && nested.module.expression_count() == 0);
    require(nested.diagnostics.front().code == DiagnosticCode::resource_limit);
    const auto default_depth = parse("f() -> " + std::string(3000, '{') + "ok" + std::string(3000, '}') + ".");
    require(default_depth.failed && default_depth.module.expression_count() == 0);
    require(default_depth.diagnostics.front().code == DiagnosticCode::resource_limit);
    std::string flat = "f() -> [ok";
    for (int i = 0; i < 8192; ++i)
        flat += ",ok";
    flat += "].";
    const auto large = parse(std::move(flat), limits);
    require(large.succeeded());
    require(std::get<ast::List>(body(large).value).elements.size() == 8193);
    const auto recovery = parse("bad() -> [ok,{bad,}].\ngood() -> [ok].");
    require(recovery.failed && recovery.module.expression_count() == 2);
}

int main() {
    aggregates();
    bounds();
}
