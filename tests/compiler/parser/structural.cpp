#include "ast/builder.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Preserve checks in every build mode, including provenance and syntax categories.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("structural expression check failed");
    }
}

// Run the real frontend and inspect its AST after source/session destruction.
ParseResult parse(std::string text, ParserLimits limits = {}) {
    SourceManager sources;
    PreprocessorSession pp(sources.add("structural.erl", std::move(text)));
    return parse_module(pp, limits);
}

// Access the last published function body, including after a failed earlier form.
const ast::Expression &body(const ParseResult &result) {
    const auto &f = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    return result.module.expression(f.clauses.front().body.front());
}

// Record identities retain contextual spelling, explicit module names and inferred syntax.
void identities() {
    const auto result = parse("f() -> {#State{}, #mod:end{}, #_{}, # _{}, R#r.a, #r.a}.");
    require(result.succeeded());
    const auto &values = std::get<ast::Tuple>(body(result).value).elements;
    const auto &local = std::get<ast::RecordExpression>(result.module.expression(values[0]).value);
    require(std::get<ast::UnresolvedRecordName>(local.identity.value).name.name == U"State");
    const auto &qualified = std::get<ast::RecordExpression>(result.module.expression(values[1]).value);
    const auto &name = std::get<ast::QualifiedRecordName>(qualified.identity.value);
    require(name.module.name == U"mod" && name.name.name == U"end");
    require(result.module.extent(qualified.identity.source).size() == 3);
    const auto &inferred = std::get<ast::RecordExpression>(result.module.expression(values[2]).value);
    require(std::holds_alternative<ast::InferredRecordName>(inferred.identity.value));
    const auto &underscore = std::get<ast::RecordExpression>(result.module.expression(values[3]).value);
    require(std::get<ast::UnresolvedRecordName>(underscore.identity.value).name.name == U"_");
    require(std::holds_alternative<ast::RecordAccess>(result.module.expression(values[4]).value));
    require(std::holds_alternative<ast::RecordIndex>(result.module.expression(values[5]).value));
}

// Keep map operations, explicit assignments and macro-generated origins in source order.
void fields() {
    const auto result = parse("-define(F, _ = default, a = 42).\nf() -> {M#{a => f(), b := X}, #r{?F}}.");
    require(result.succeeded());
    const auto &values = std::get<ast::Tuple>(body(result).value).elements;
    const auto &map = std::get<ast::MapExpression>(result.module.expression(values[0]).value);
    require(map.base.has_value() && map.fields.size() == 2);
    require(map.fields[0].kind == ast::MapFieldKind::associate && map.fields[1].kind == ast::MapFieldKind::exact);
    const auto &anchor = result.module.anchor(map.fields[0].source);
    require(anchor.spelling.source->text.substr(anchor.spelling.begin, anchor.spelling.end - anchor.spelling.begin) ==
            U"=>");
    const auto &record = std::get<ast::RecordExpression>(result.module.expression(values[1]).value);
    require(!record.base && record.fields.size() == 2);
    require(std::get<ast::Variable>(record.fields[0].name).name == U"_");
    require(std::get<ast::Atom>(record.fields[1].name).name == U"a");
    require(!result.module.anchor(record.fields[0].source).related.empty());
}

// Structural postfix chains iterate, while nested values retain the configured depth bound.
void limits_and_recovery() {
    ParserLimits limits;
    limits.nesting = 8;
    std::string flat = "f() -> M";
    for (int i = 0; i < 4096; ++i) {
        flat += "#{a => 1}";
    }
    require(parse(flat + ".", limits).succeeded());
    std::string nested = "f() -> ";
    for (int i = 0; i < 100; ++i) {
        nested += "#{a => ";
    }
    nested += "ok" + std::string(100, '}') + ".";
    const auto exhausted = parse(nested, limits);
    require(exhausted.failed && exhausted.module.expression_count() == 0);
    require(exhausted.diagnostics.front().code == DiagnosticCode::resource_limit);
    const auto recovery = parse("bad(X) -> #r{a = 1,}.\ngood() -> #{}.");
    require(recovery.failed && recovery.module.expression_count() == 1 && recovery.module.pattern_count() == 0);
}

// Child and field sources cannot cross form/owner boundaries during AST construction.
void invariants() {
    ast::Builder a;
    ast::Builder b;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic", 1, 1}, {}};
    auto first = a.begin({}, eof);
    auto second = b.begin({}, eof);
    const auto source = a.source(0, 0, 0);
    const auto other = b.source(0, 0, 0);
    const auto atom = a.expression(ast::Atom{U"ok"}, source);
    bool rejected = false;
    try {
        a.expression(ast::MapExpression{{}, {{ast::MapFieldKind::exact, atom, atom, other}}}, source);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
    rejected = false;
    try {
        b.expression(ast::RecordExpression{{}, {ast::InferredRecordName{}, other}, {{{ast::Atom{U"a"}}, atom, other}}},
                     other);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
}

int main() {
    identities();
    fields();
    limits_and_recovery();
    invariants();
}
