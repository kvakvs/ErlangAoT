#include "bindings.hpp"
#include "capabilities.hpp"
#include "features.hpp"

namespace erlang_aot::semantic {
namespace {
using Parameters = std::map<std::u32string, std::size_t>;

// Wildcards consume argument positions independently, while named parameters must be distinct.
void parameter(const Module &module, const ast::PatternSyntaxId &id, const std::size_t position, Parameters &names,
               const Reporter &out) {
    const auto &pattern = module.syntax->pattern(id);
    const auto expression = std::visit([](const auto &value) { return value.expression; }, pattern.value);
    const auto &syntax = module.syntax->expression(ungroup(*module.syntax, expression)).value;
    const auto *variable = std::get_if<ast::Variable>(&syntax);
    if (!variable || variable->name == U"_") {
        return;
    }
    if (!names.emplace(variable->name, position).second) {
        reject_capability(module, pattern.source, "pattern matching", out);
    }
}

// Bind only value reads; literal call target atoms are handled by call resolution.
void reference(const Module &module, Function &function, const ast::ExprId &id, const Parameters &names,
               const Reporter &out) {
    const auto &expression = module.syntax->expression(id);
    const auto *variable = std::get_if<ast::Variable>(&expression.value);
    if (!variable) {
        return;
    }
    if (variable->name == U"_") {
        report(module, &expression.source, "wildcard '_' cannot be read", out);
        return;
    }
    const auto found = names.find(variable->name);
    if (found == names.end()) {
        report(module, &expression.source, "unbound variable " + utf8(variable->name), out);
        return;
    }
    function.bindings.push_back({id, found->second});
}

// Iterative traversal preserves source-order reads without adding a host recursion limit.
void bind_body(const Module &module, Function &function, const ast::FunctionClause &clause, const Parameters &names,
               const Reporter &out) {
    auto pending = clause.body;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        reference(module, function, id, names, out);
        const auto children = expression_children(module.syntax->expression(id));
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
}

// A failed capability pass cannot leave partial bindings for an unsupported multi-clause function.
void bind_function(Module &module, Function &function, const Reporter &out) {
    function.bindings.clear();
    const auto &declaration = std::get<ast::Function>(module.syntax->form(function.form).value);
    if (declaration.clauses.size() != 1) {
        return;
    }
    const auto &clause = declaration.clauses.front();
    Parameters names;
    for (std::size_t i = 0; i < clause.arguments.size(); ++i) {
        parameter(module, clause.arguments[i], i, names, out);
    }
    bind_body(module, function, clause, names, out);
}
} // namespace

void bind_parameters(Module &module, const Reporter &out) {
    for (auto &function : module.functions) {
        bind_function(module, function, out);
    }
}
} // namespace erlang_aot::semantic
