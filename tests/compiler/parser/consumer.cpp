#include <erlang_aot/compiler/parser.hpp>
#include <stdexcept>

using namespace erlang_aot;

// Embedders may return the owner without retaining lexer or preprocessing sessions.
ast::Module load() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("consumer.erl", "-define(V,42). f() -> ?V."));
    auto parsed = parse_module(pp);
    if (!parsed.succeeded())
        throw std::runtime_error("consumer parse failed");
    return std::move(parsed.module);
}

int main() {
    const auto module = load();
    const auto &function = std::get<ast::Function>(module.form(module.forms().back()).value);
    const auto &expression = module.expression(function.clauses.front().body.front());
    const auto &origin = module.anchor(expression.source);
    if (std::get<ast::IntegerLiteral>(expression.value).value.decimal != "42" ||
        origin.location.file != "consumer.erl" || origin.related.empty() || module.extent(expression.source).empty() ||
        !module.features(module.forms().back()) || !module.features())
        throw std::runtime_error("consumer lost syntax or provenance");
    std::size_t forms = 0;
    for (const auto &id : module.forms())
        module.visit(id, [&forms](const auto &) { ++forms; });
    if (forms != 2)
        throw std::runtime_error("consumer traversal lost forms");
}
