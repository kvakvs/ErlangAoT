#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace erlang_aot;
static_assert(!std::is_convertible_v<ast::ExprId, ast::TermId>);

// Keep assertions active and identify the failed contract without debugger setup.
void require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition)
        throw std::runtime_error("attribute check at line " + std::to_string(location.line()));
}

// Exercise parse_form directly so file documentation remains the parser's responsibility.
ParseResult raw(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    Lexer lexer(sources.add("attributes.erl", std::move(text)));
    std::vector<Token> tokens;
    ParserSession parser(limits);
    while (auto token = lexer.next()) {
        tokens.push_back(std::move(*token));
        if (tokens.back().kind == TokenKind::dot) {
            parser.parse_form(tokens, tokens.back());
            tokens.clear();
        }
    }
    return std::move(parser).finish();
}

// Preserve literal data, unevaluated defaults, documentation paths and equiv calls.
void payloads() {
    auto result = raw("-module(m,[A]). -custom({f/2, #{x => 1, x => 2}}). "
                      "-record #Point{field = run()}. -doc({file, \"missing.md\"}). "
                      "-doc(#{since => \"29\", equiv => m:f(X), since => \"30\"}).");
    require(result.succeeded() && result.module.forms().size() == 5);
    const auto &module = result.module;
    require(std::get<ast::ModuleAttribute>(module.form(module.forms()[0]).value).parameters->size() == 1);
    const auto &attribute = std::get<ast::GenericAttribute>(module.form(module.forms()[1]).value);
    require(std::get<ast::TermTuple>(module.term(attribute.value).value).elements.size() == 2);
    const auto &record = std::get<ast::RecordDeclaration>(module.form(module.forms()[2]).value);
    require(record.native && record.name.name == U"Point");
    require(std::holds_alternative<ast::CallExpression>(module.expression(*record.fields[0].default_value).value));
    const auto &doc = std::get<ast::DocumentationAttribute>(module.form(module.forms()[3]).value);
    require(std::holds_alternative<ast::TermTuple>(module.term(std::get<ast::TermId>(doc.value)).value));
    const auto &metadata = std::get<std::vector<ast::DocumentationEntry>>(
        std::get<ast::DocumentationAttribute>(module.form(module.forms()[4]).value).value);
    require(metadata.size() == 2 && std::holds_alternative<ast::ExprId>(metadata[0].value));
    std::ostringstream tree;
    print_ast(tree, module);
    require(tree.str().find("TermTuple") != std::string::npos);
    require(tree.str().find("RecordDeclaration name='Point' native=1") != std::string::npos);
    require(module.anchor(module.term(attribute.value).source).spelling.source != nullptr);
}

// Failed forms reclaim both syntax and term arenas and parsing resumes at the next form.
void recovery() {
    const auto result = raw("-custom({ok, run()}). -doc(#{a => ok, b => run()}). -custom(ok).");
    require(result.failed && result.diagnostics.size() == 2 && result.module.forms().size() == 1);
    require(result.module.expression_count() == 0 && result.module.term_count() == 1);
    ParserLimits limits;
    limits.nodes = 5;
    const auto bounded = raw("-custom([1,2,3,4,5,6]).", limits);
    require(bounded.failed && bounded.module.term_count() == 0);
    require(bounded.diagnostics[0].code == DiagnosticCode::resource_limit);
}

// Literal IDs retain ownership and become stale after transactional rollback.
void ownership() {
    ast::Builder builder;
    std::optional<ast::TermId> stale;
    {
        auto transaction = builder.begin({}, Token{});
        stale = builder.term(ast::Atom{U"old"}, builder.source(0, 0, 0));
    }
    auto transaction = builder.begin({}, Token{});
    const auto source = builder.source(0, 0, 0);
    const auto fresh = builder.term(ast::Atom{U"new"}, source);
    bool rejected = false;
    try {
        builder.term(ast::TermTuple{{*stale}}, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    transaction.commit(builder.form(ast::GenericAttribute{{U"custom"}, fresh}, source));
    auto module = std::move(builder).finish();
    auto moved = std::move(module);
    require(std::get<ast::Atom>(moved.term(fresh).value).name == U"new");
}

int main() {
    payloads();
    recovery();
    ownership();
}
