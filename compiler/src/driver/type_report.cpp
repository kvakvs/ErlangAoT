#include "type_report.hpp"
#include "../semantic/types/printing.hpp"
#include "display.hpp"
#include <erlang_aot/compiler/printing.hpp>
#include <iostream>
#include <set>
#include <stdexcept>

// --print-types (docs/semantic.md#--print-types): each module as Erlang source, its functions headed by their
// inferred signatures and its expressions annotated `Expression :: Type` where inference knows more than term().
namespace erlang_aot::cli {
namespace {
namespace types = semantic::types;

// Functions with a specification of their own, by module, name and arity.
std::set<types::Key> specified_functions(const types::Registry &registry) {
    std::set<types::Key> result;
    for (const auto &contract : registry.contracts) {
        if (!contract.callback) {
            result.insert(contract.key);
        }
    }
    return result;
}

// Literals, signed numbers included, already show their type; a match's value is its right side's, which carries
// the annotation.
bool self_describing(const ast::Module &syntax, const ast::ExprValue &value) {
    if (const auto *unary = std::get_if<ast::UnaryExpression>(&value)) {
        const auto &operand = syntax.expression(unary->operand).value;
        return std::holds_alternative<ast::IntegerLiteral>(operand) ||
               std::holds_alternative<ast::FloatLiteral>(operand);
    }
    return std::holds_alternative<ast::Atom>(value) || std::holds_alternative<ast::IntegerLiteral>(value) ||
           std::holds_alternative<ast::FloatLiteral>(value) || std::holds_alternative<ast::CharacterLiteral>(value) ||
           std::holds_alternative<ast::StringLiteral>(value) || std::holds_alternative<ast::MatchExpression>(value);
}

// A fact as annotation text: its type, and the argument it equals (1-based) when inference proved one.
std::string fact_source(const types::Inference &inferred, const types::Fact &fact) {
    const bool known = inferred.graph.get(fact.type).kind != types::Kind::top;
    auto type = known ? types::type_source(inferred.graph, fact.type) : std::string();
    if (!fact.argument) {
        return type;
    }
    const auto argument = "argument " + std::to_string(*fact.argument + 1);
    return known ? type + " (" + argument + ')' : argument;
}

// The annotation of an expression: none for literals, for facts that say nothing, and for the argument relation of
// a variable, which its name already shows.
std::optional<std::string> expression_note(const ast::Module &syntax, const types::Inference &inferred,
                                           const ast::Expression &expression) {
    const auto found = inferred.expressions.find(&expression);
    if (found == inferred.expressions.end() || self_describing(syntax, expression.value)) {
        return std::nullopt;
    }
    auto fact = found->second;
    if (std::holds_alternative<ast::Variable>(expression.value)) {
        fact.argument.reset();
    }
    auto text = fact_source(inferred, fact);
    return text.empty() ? std::nullopt : std::optional{std::move(text)};
}

// `name(Inputs) -> Result` of a function summary.
std::string signature(const types::Inference &inferred, const std::string &name, const types::Summary &summary) {
    std::string inputs;
    for (const auto input : summary.inputs) {
        inputs += inputs.empty() ? "" : ", ";
        inputs += types::type_source(inferred.graph, input);
    }
    const auto result = fact_source(inferred, summary.result);
    return atom_source(name) + '(' + inputs + ") -> " + (result.empty() ? "term()" : result);
}

// The comment above a function: its inferred signature, and whether a specification declares it.
std::vector<std::string> function_note(const semantic::Module &module, const types::Inference &inferred,
                                       const std::set<types::Key> &specified, const ast::Form &form) {
    for (const auto &function : module.functions) {
        if (&module.syntax->form(function.form) != &form) {
            continue;
        }
        const auto name = utf8(function.key.name);
        const bool declared = specified.contains({utf8(module.name), name, function.key.arity});
        return {"inferred: " + signature(inferred, name, inferred.functions.at(&function)) +
                (declared ? "  (declared by -spec)" : "")};
    }
    return {};
}

// "complete", or "widened" when a limit made the analysis give up precision.
std::string_view completeness(const types::Graph &graph) { return graph.widened() ? "widened" : "complete"; }
} // namespace

void print_types(const Analysis &analysis, const codegen::CompilationRequest &request) {
    if (!analysis.declared || !analysis.inferred) {
        throw std::logic_error("type inspection requires completed analysis");
    }
    const auto &inferred = *analysis.inferred;
    const auto specified = specified_functions(*analysis.declared);
    for (const auto &module : analysis.modules) {
        std::cout << "%% module " << quote_text(utf8(module->name)) << " source=" << quote_text(module->file)
                  << " target=" << quote_text(request.project_target)
                  << " declared=" << completeness(analysis.declared->graph)
                  << " inferred=" << completeness(inferred.graph) << '\n';
        const SourceNotes notes{
            .expression =
                [&](const ast::Expression &expression) {
                    return expression_note(*module->syntax, inferred, expression);
                },
            .form = [&](const ast::Form &form) { return function_note(*module, inferred, specified, form); }};
        print_source(std::cout, *module->syntax, notes);
        std::cout << '\n';
    }
    if (!std::cout) {
        throw std::runtime_error("cannot write type inspection output");
    }
}
} // namespace erlang_aot::cli
