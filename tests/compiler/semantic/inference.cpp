#include "semantic/types/inference.hpp"
#include "semantic/bindings.hpp"
#include "semantic/capabilities.hpp"
#include "semantic/services.hpp"
#include "semantic/types/declarations.hpp"
#include <clause/compiler/parser.hpp>
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace clause;
using namespace clause::semantic;
namespace t = clause::semantic::types;

// Keep these relational and budget invariants until public type inspection replaces them.
void require(bool value, const std::source_location site = std::source_location::current()) {
    if (!value) {
        throw std::runtime_error("inference invariant at line " + std::to_string(site.line()));
    }
}

// Real source proves annotations stay independent of constants and parameter projections.
void local_inference() {
    SourceManager sources;
    PreprocessorSession pp(sources.add("facts.erl", R"(
-module(facts).
-export([constant/0,id/1,project/3]).
-spec constant() -> atom().
constant() -> -42.
id(X) -> (X).
-spec project(integer(),atom(),term()) -> atom().
project(_,Y,_) -> Y.
unknown(X) -> id(X).
)"));
    auto parsed = parse_module(pp);
    require(!parsed.failed);
    const Reporter report = [](const Diagnostic &d) { require(d.severity != Severity::error); };
    std::vector<std::unique_ptr<Module>> modules;
    modules.push_back(index(parsed.module, "facts.erl", report));
    auto &module = *modules.front();
    bind_parameters(module, report);
    resolve_services(module, report);
    check_capabilities(module, report);
    const auto calls = resolve_calls(modules, report);
    const auto declared = t::resolve_declarations(modules, report);
    const auto inferred = t::infer(calls);
    require(declared->contracts.size() == 2 && !inferred->graph.widened());
    const auto &constant = inferred->functions.at(&module.functions[0]);
    require(inferred->graph.get(constant.result.type).name == "-42" && !constant.result.argument);
    const auto &identity = inferred->functions.at(&module.functions[1]);
    require(identity.inputs == std::vector{inferred->graph.top()});
    require(identity.result.type == inferred->graph.top() && identity.result.argument == 0);
    const auto &projection = inferred->functions.at(&module.functions[2]);
    require(projection.inputs == std::vector(3, inferred->graph.top()) && projection.result.argument == 1);
    require(inferred->functions.at(&module.functions[3]).result.argument == 0);
    const auto bounded = t::infer(calls, {.syntax_work = 0});
    require(bounded->graph.widened());
    for (const auto &[function, summary] : bounded->functions) {
        require(function != nullptr && summary.result.type == bounded->graph.top() && !summary.result.argument);
    }
    const auto node_limited = t::infer(calls, {.nodes = 2});
    require(node_limited->graph.widened());
    require(node_limited->functions.at(&module.functions[0]).result.type == node_limited->graph.top());
}

int main() {
    try {
        local_inference();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected inference failure\n", stderr);
        return 1;
    }
}
