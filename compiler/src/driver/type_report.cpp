#include "type_report.hpp"
#include "../semantic/capabilities.hpp"
#include "../semantic/types/printing.hpp"
#include "display.hpp"
#include <clause/compiler/printing.hpp>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>

// --print-types (docs/semantic.md#--print-types): each module as Erlang source, its functions headed by their
// declared and inferred signatures and each line's outermost expression noted `% Type` where inference knows more than
// term().
namespace clause::cli {
namespace {
namespace types = semantic::types;

// The specification of each function that has one, by module, name and arity.
std::map<types::Key, const types::Contract *> specifications(const types::Registry &registry) {
    std::map<types::Key, const types::Contract *> result;
    for (const auto &contract : registry.contracts) {
        if (!contract.callback) {
            result.emplace(contract.key, &contract);
        }
    }
    return result;
}

// Adds the operands of a literal term to `pending`: literals have none, tuples, lists, constructed maps and
// bitstrings their elements; false for an expression that is no literal term.
struct LiteralOperands {
    const ast::Module &syntax;
    std::vector<ast::ExprId> &pending;

    template <typename T> bool operator()(const T &) const { return false; }

    bool operator()(const ast::Atom &) const { return true; }

    bool operator()(const ast::IntegerLiteral &) const { return true; }

    bool operator()(const ast::FloatLiteral &) const { return true; }

    bool operator()(const ast::CharacterLiteral &) const { return true; }

    bool operator()(const ast::StringLiteral &) const { return true; }

    bool operator()(const ast::Group &value) const { return add({value.expression}); }

    bool operator()(const ast::Tuple &value) const { return add(value.elements); }

    bool operator()(const ast::UnaryExpression &value) const {
        const auto &operand = syntax.expression(value.operand).value;
        return std::holds_alternative<ast::IntegerLiteral>(operand) ||
               std::holds_alternative<ast::FloatLiteral>(operand);
    }

    bool operator()(const ast::List &value) const { return add(value.elements) && (!value.tail || add({*value.tail})); }

    bool operator()(const ast::MapExpression &value) const {
        for (const auto &field : value.fields) {
            add({field.key, field.value});
        }
        return !value.base;
    }

    bool operator()(const ast::Bitstring &value) const {
        for (const auto &segment : value.segments) {
            add({segment.value});
            if (segment.size) {
                add({*segment.size});
            }
        }
        return true;
    }

    // Queue operands; always true.
    bool add(const std::vector<ast::ExprId> &operands) const {
        pending.insert(pending.end(), operands.begin(), operands.end());
        return true;
    }
};

// Literal terms, signed numbers included, already show their type.
bool self_describing(const ast::Module &syntax, const ast::ExprValue &value) {
    std::vector<ast::ExprId> pending;
    if (!std::visit(LiteralOperands{syntax, pending}, value)) {
        return false;
    }
    while (!pending.empty()) {
        const auto &operand = syntax.expression(pending.back()).value;
        pending.pop_back();
        if (!std::visit(LiteralOperands{syntax, pending}, operand)) {
            return false;
        }
    }
    return true;
}

// The names a function's arguments print as: the variable the first clause binding the whole argument gives it,
// else (or when an earlier argument took that name) `_argumentN`, 1-based; both are type variables.
using Names = std::vector<std::string>;

Names argument_names(const ast::Module &syntax, const ast::Function &definition) {
    const auto arity = definition.clauses.front().arguments.size();
    Names names(arity);
    std::set<std::string> taken;
    for (std::size_t position = 0; position < arity; ++position) {
        for (const auto &clause : definition.clauses) {
            const auto root = semantic::ungroup(syntax, semantic::pattern_root(syntax, clause.arguments[position]));
            const auto *variable = std::get_if<ast::Variable>(&syntax.expression(root).value);
            if (variable && variable->name != U"_" && !taken.contains(utf8(variable->name))) {
                names[position] = utf8(variable->name);
                break;
            }
        }
        if (names[position].empty()) {
            names[position] = "_argument" + std::to_string(position + 1);
        }
        taken.insert(names[position]);
    }
    return names;
}

// The argument names of the function owning each expression of a module's functions.
std::map<const ast::Expression *, Names> expression_owners(const semantic::Module &module) {
    std::map<const ast::Expression *, Names> result;
    for (const auto &function : module.functions) {
        const auto &definition = std::get<ast::Function>(module.syntax->form(function.form).value);
        const auto names = argument_names(*module.syntax, definition);
        auto pending = semantic::function_roots(definition);
        while (!pending.empty()) {
            const auto &expression = module.syntax->expression(pending.back());
            pending.pop_back();
            result.try_emplace(&expression, names);
            const auto children = semantic::expression_children(module, expression);
            pending.insert(pending.end(), children.begin(), children.end());
        }
    }
    return result;
}

// The module's tuple records, which tuple facts of their name and size print as.
types::RecordFields record_fields(const semantic::Module &module) {
    types::RecordFields result;
    for (const auto &[name, layout] : module.records) {
        if (layout.native) {
            continue;
        }
        std::vector<std::string> fields;
        fields.reserve(layout.fields.size());
        for (const auto &field : layout.fields) {
            fields.push_back(utf8(field.name.name));
        }
        result.emplace(std::pair{utf8(name), layout.fields.size() + 1}, std::move(fields));
    }
    return result;
}

// A dependent fact as annotation text, a function type of its construct's name over its parameters
// (docs/semantic.md#dependent-facts).
std::string dependent_text(const types::Inference &inferred, const types::Dependent &dependent, const Names *names,
                           const types::RecordFields &records) {
    types::DependentText text{types::operator_name(dependent.construct), dependent.names, {}};
    for (const auto &type : dependent.types) {
        text.types.push_back({type.inputs, type.result.type, type.result.argument});
    }
    return types::dependent_source(
        inferred.graph, text, names ? std::span<const std::string>(*names) : std::span<const std::string>(), &records);
}

// A fact as annotation text: its type, or the name of the argument it equals when only that relation is known.
std::string fact_source(const types::Inference &inferred, const types::Fact &fact, const Names *names,
                        const types::RecordFields &records) {
    if (fact.dependent) {
        return dependent_text(inferred, inferred.dependents.at(*fact.dependent), names, records);
    }
    if (inferred.graph.get(fact.type).kind != types::Kind::top) {
        return types::type_source(inferred.graph, fact.type, 1024, &records);
    }
    if (!fact.argument) {
        return {};
    }
    return names && *fact.argument < names->size() ? names->at(*fact.argument)
                                                   : "_argument" + std::to_string(*fact.argument + 1);
}

// The annotation of an expression: none for literals, for facts that say nothing, and for the argument relation of
// a variable, which its name already shows.
std::optional<std::string> expression_note(const ast::Module &syntax, const types::Inference &inferred,
                                           const ast::Expression &expression, const Names *names,
                                           const types::RecordFields &records) {
    const auto found = inferred.expressions.find(&expression);
    if (found == inferred.expressions.end() || self_describing(syntax, expression.value)) {
        return std::nullopt;
    }
    auto fact = found->second;
    if (std::holds_alternative<ast::Variable>(expression.value)) {
        fact.argument.reset();
    }
    auto text = fact_source(inferred, fact, names, records);
    return text.empty() ? std::nullopt : std::optional{std::move(text)};
}

// `name(Inputs) -> Result` of a function summary, or one signature per function type when it keeps several:
// `f(integer()) -> integer(); (atom()) -> string()`. An argument a result equals shows its name, as a type variable.
std::string signature(const types::Inference &inferred, const std::string &name, const types::Summary &summary,
                      const Names &names, const types::RecordFields &records) {
    std::vector<types::FunctionText> texts;
    if (summary.types.size() > 1) {
        for (const auto &type : summary.types) {
            texts.push_back({type.inputs, type.result.type, type.result.argument});
        }
    } else {
        texts.push_back({summary.inputs, summary.result.type, summary.result.argument});
    }
    return atom_source(name) + types::function_source(inferred.graph, texts, names, &records);
}

// `name(Inputs) -> Result when Constraints` of one overload of a resolved specification.
std::string declared_signature(const types::Graph &graph, const std::string &name, const types::Overload &overload) {
    const auto &node = graph.get(overload.function);
    if (node.kind != types::Kind::function || node.children.empty()) {
        return atom_source(name) + "(...) -> term()";
    }
    std::string inputs = node.name == "any_arguments" ? "..." : "";
    for (const auto input : std::span(node.children).first(node.children.size() - 1)) {
        inputs += inputs.empty() ? "" : ", ";
        inputs += types::type_source(graph, input);
    }
    auto text = atom_source(name) + '(' + inputs + ") -> " + types::type_source(graph, node.children.back());
    std::string constraints;
    for (const auto &[variable, bound] : overload.constraints) {
        constraints += constraints.empty() ? " when " : ", ";
        constraints += variable + " :: " + types::type_source(graph, bound);
    }
    return text + constraints;
}

// The comment above a function: its specification's overloads, if any, then its inferred signature.
std::vector<std::string> function_note(const semantic::Module &module, const Analysis &analysis,
                                       const std::map<types::Key, const types::Contract *> &specified,
                                       const types::RecordFields &records, const ast::Form &form) {
    for (const auto &function : module.functions) {
        if (&module.syntax->form(function.form) != &form) {
            continue;
        }
        const auto name = utf8(function.key.name);
        std::vector<std::string> lines;
        if (const auto found = specified.find({utf8(module.name), name, function.key.arity});
            found != specified.end()) {
            for (const auto &overload : found->second->overloads) {
                lines.push_back("declared: " + declared_signature(analysis.declared->graph, name, overload));
            }
        }
        const auto &inferred = *analysis.inferred;
        const auto names = argument_names(*module.syntax, std::get<ast::Function>(form.value));
        lines.push_back("inferred: " + signature(inferred, name, inferred.functions.at(&function), names, records));
        return lines;
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
    const auto specified = specifications(*analysis.declared);
    for (const auto &module : analysis.modules) {
        std::cout << "%% module " << quote_text(utf8(module->name)) << " source=" << quote_text(module->file)
                  << " target=" << quote_text(request.project_target)
                  << " declared=" << completeness(analysis.declared->graph)
                  << " inferred=" << completeness(inferred.graph) << '\n';
        const auto owners = expression_owners(*module);
        const auto records = record_fields(*module);
        const SourceNotes notes{
            .expression =
                [&](const ast::Expression &expression) {
                    const auto owner = owners.find(&expression);
                    return expression_note(*module->syntax, inferred, expression,
                                           owner == owners.end() ? nullptr : &owner->second, records);
                },
            .form = [&](const ast::Form &form) { return function_note(*module, analysis, specified, records, form); }};
        print_source(std::cout, *module->syntax, notes);
        std::cout << '\n';
    }
    if (!std::cout) {
        throw std::runtime_error("cannot write type inspection output");
    }
}
} // namespace clause::cli
