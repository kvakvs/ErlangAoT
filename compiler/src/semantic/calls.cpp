#include "calls.hpp"
#include "capabilities.hpp"
#include "features.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
namespace {
using Modules = std::map<std::u32string, Module *>;

// Resolve literal remote names without ever falling back to another project target.
Module *call_module(const FunctionRef caller, const ast::RemoteExpression &remote, const Modules &modules,
                    const ast::NodeSource &source, const Reporter &out) {
    const auto &name =
        std::get<ast::Atom>(caller.module->syntax->expression(ungroup(*caller.module->syntax, remote.module)).value)
            .name;
    const auto found = modules.find(name);
    if (found == modules.end()) {
        report(*caller.module, &source, "unknown module " + utf8(name), out);
        return nullptr;
    }
    return found->second;
}

// Match name/arity first, then enforce remote visibility even for self-qualified calls.
std::optional<FunctionRef> callee(const FunctionRef caller, const ast::CallExpression &call, const Modules &modules,
                                  const ast::NodeSource &source, const Reporter &out) {
    const auto &syntax = *caller.module->syntax;
    const auto &target = syntax.expression(ungroup(syntax, call.target)).value;
    const auto *remote = std::get_if<ast::RemoteExpression>(&target);
    auto *owner = remote ? call_module(caller, *remote, modules, source, out) : caller.module;
    if (!owner) {
        return {};
    }
    const auto &name = remote ? std::get<ast::Atom>(syntax.expression(ungroup(syntax, remote->function)).value)
                              : std::get<ast::Atom>(target);
    const FunctionKey key{name.name, call.arguments.size()};
    const auto found = owner->lookup.find(key);
    if (found == owner->lookup.end()) {
        report(*caller.module, &source,
               "undefined function " + utf8(owner->name) + ":" + utf8(key.name) + "/" + std::to_string(key.arity), out);
        return {};
    }
    auto &function = owner->functions.at(found->second);
    if (remote && !function.exported) {
        report(*caller.module, &source, "remote function is not exported: " + utf8(key.name), out);
        return {};
    }
    return FunctionRef{owner, &function};
}

// Walk all accepted bodies iteratively; source-order dependencies remain deterministic.
void body(CallGraph &graph, const FunctionRef caller, const Modules &modules, const Reporter &out) {
    const auto &syntax = *caller.module->syntax;
    auto pending = std::get<ast::Function>(syntax.form(caller.function->form).value).clauses.front().body;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = syntax.expression(id);
        if (const auto *call = std::get_if<ast::CallExpression>(&expression.value);
            call && !caller.function->services.contains(&expression)) {
            if (const auto resolved = callee(caller, *call, modules, expression.source, out)) {
                graph.calls.push_back({id, caller, *resolved});
            }
        }
        const auto children = expression_children(expression);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
}

// Index every module before resolving any call, preserving forward references.
Modules module_index(std::span<const std::unique_ptr<Module>> modules, const Reporter &out) {
    Modules result;
    for (const auto &module : modules) {
        if (!result.emplace(module->name, module.get()).second) {
            report(*module, &module->syntax->form(*module->declaration).source,
                   "duplicate module in compilation batch: " + utf8(module->name), out);
        }
    }
    return result;
}

struct Dependencies {
    // Count each edge once and retain reverse users for iterative topological ordering.
    std::vector<std::size_t> remaining;
    std::vector<std::vector<std::size_t>> users;
};

// Translate stable declaration pointers to dense indices without changing identity.
Dependencies dependencies(const CallGraph &graph, const std::vector<FunctionRef> &functions) {
    std::map<Function *, std::size_t> indices;
    for (std::size_t i = 0; i < functions.size(); ++i) {
        indices.emplace(functions[i].function, i);
    }
    Dependencies result{std::vector<std::size_t>(functions.size()),
                        std::vector<std::vector<std::size_t>>(functions.size())};
    for (const auto &call : graph.calls) {
        const auto caller = indices.at(call.caller.function);
        ++result.remaining[caller];
        result.users[indices.at(call.callee.function)].push_back(caller);
    }
    return result;
}

// A partial order cannot be consumed when any function depends on a recursive component.
void reject_cycle(CallGraph &graph, const Dependencies &edges, const std::vector<FunctionRef> &functions,
                  const Reporter &out) {
    if (graph.order.size() == functions.size()) {
        return;
    }
    const auto blocked = static_cast<std::size_t>(
        std::ranges::find_if(edges.remaining, [](auto count) { return count != 0; }) - edges.remaining.begin());
    const auto &ref = functions[blocked];
    reject_capability(*ref.module, ref.module->syntax->form(ref.function->form).source, "recursive calls", out);
    graph.order.clear();
}

// Kahn's algorithm avoids host-stack recursion and counts repeated dependency edges exactly.
void order(CallGraph &graph, const std::vector<FunctionRef> &functions, const Reporter &out) {
    auto edges = dependencies(graph, functions);
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < edges.remaining.size(); ++i) {
        if (edges.remaining[i] == 0) {
            ready.push_back(i);
        }
    }
    for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
        const auto next = ready[cursor];
        graph.order.push_back(functions[next]);
        for (const auto user : edges.users[next]) {
            if (--edges.remaining[user] == 0) {
                ready.push_back(user);
            }
        }
    }
    reject_cycle(graph, edges, functions, out);
}

} // namespace

CallGraph resolve_calls(std::span<const std::unique_ptr<Module>> modules, const Reporter &out) {
    const auto names = module_index(modules, out);
    CallGraph graph;
    std::vector<FunctionRef> functions;
    for (const auto &module : modules) {
        for (auto &function : module->functions) {
            functions.push_back({module.get(), &function});
            body(graph, functions.back(), names, out);
        }
    }
    order(graph, functions, out);
    return graph;
}
} // namespace erlang_aot::semantic
