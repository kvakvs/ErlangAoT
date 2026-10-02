#include "type_report.hpp"
#include "../semantic/capabilities.hpp"
#include "display.hpp"
#include "type_declarations.hpp"
#include "type_format.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace erlang_aot::cli {
namespace {
namespace types = semantic::types;

// Index contract presence once without changing the source-ordered function report.
std::set<types::Key> specified_functions(const types::Registry &registry) {
    std::set<types::Key> result;
    for (const auto &contract : registry.contracts) {
        if (!contract.callback) {
            result.insert(contract.key);
        }
    }
    return result;
}

// Keep original logical locations, including includes and file attributes, attached to each fact.
std::string location(const ast::Module &syntax, const ast::NodeSource &source) {
    const auto &anchor = syntax.anchor(source).location;
    return quote_text(anchor.file) + ':' + std::to_string(anchor.line) + ':' + std::to_string(anchor.column);
}

// External inputs remain conservative even when a user specification describes a narrower type.
std::string input_text(const types::Inference &inferred, const types::Summary &summary) {
    std::string text = "[";
    for (std::size_t i = 0; i < summary.inputs.size(); ++i) {
        if (i != 0) {
            text += ", ";
        }
        text += fact_text(inferred, {summary.inputs[i], {}});
    }
    return text + ']';
}

// Identify read facts by stable clause-local slots rather than by variable spelling.
std::string binding_text(const semantic::Function &function, const ast::ExprId &id) {
    const auto found = std::ranges::find(function.bindings, id, &semantic::Binding::expression);
    if (found == function.bindings.end() || found->use != semantic::BindingUse::read) {
        return {};
    }
    return " binding=clause[" + std::to_string(found->identity.clause) + "].local[" +
           std::to_string(found->identity.local) + ']';
}

// Traverse each supported expression in source order instead of iterating pointer-keyed inference maps.
void expressions(const semantic::Module &module, const semantic::Function &function, const types::Inference &inferred) {
    const auto &syntax = *module.syntax;
    const auto &definition = std::get<ast::Function>(syntax.form(function.form).value);
    auto pending = semantic::function_roots(definition);
    std::ranges::reverse(pending);
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = syntax.expression(id);
        const auto found = inferred.expressions.find(&expression);
        if (found != inferred.expressions.end()) {
            std::cout << "    expression " << location(syntax, expression.source)
                      << " inferred=" << fact_text(inferred, found->second) << binding_text(function, id) << '\n';
        }
        const auto children = semantic::expression_children(expression);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
}

// Keep declaration provenance alongside independent inferred inputs/results and exact parameter relations.
void functions(const semantic::Module &module, const types::Inference &inferred,
               const std::set<types::Key> &specified) {
    const auto owner = utf8(module.name);
    for (const auto &function : module.functions) {
        const auto &summary = inferred.functions.at(&function);
        const auto name = utf8(function.key.name);
        const bool declared = specified.contains({owner, name, function.key.arity});
        std::cout << "  function " << quote_text(name) << '/' << function.key.arity << " at "
                  << location(*module.syntax, module.syntax->form(function.form).source)
                  << " declared=" << (declared ? "spec" : "none")
                  << " inferred inputs=" << input_text(inferred, summary)
                  << " result=" << fact_text(inferred, summary.result) << '\n';
        expressions(module, function, inferred);
    }
}
} // namespace

void print_types(const Analysis &analysis, const codegen::CompilationRequest &request) {
    if (!analysis.declared || !analysis.inferred) {
        throw std::logic_error("type inspection requires completed analysis");
    }
    const auto specified = specified_functions(*analysis.declared);
    for (const auto &module : analysis.modules) {
        std::cout << "module " << quote_text(utf8(module->name)) << " source=" << quote_text(module->file)
                  << " target=" << quote_text(request.project_target) << '\n';
        std::cout << "  analysis declared=" << (analysis.declared->graph.widened() ? "widened" : "complete")
                  << " inferred=" << (analysis.inferred->graph.widened() ? "widened" : "complete") << '\n';
        print_declared_types(std::cout, *analysis.declared, *module);
        functions(*module, *analysis.inferred, specified);
    }
    if (!std::cout) {
        throw std::runtime_error("cannot write type inspection output");
    }
}
} // namespace erlang_aot::cli
