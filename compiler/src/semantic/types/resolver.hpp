#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic::types {
using Uses = std::map<std::string, std::size_t>;

struct Scope {
    // Each declaration/overload has independent quantified variables and use counts.
    std::string identity;
    Uses variables;
    // An unfinished syntax walk cannot justify singleton-variable errors.
    bool incomplete = false;
};

struct Resolver {
    // Borrow batch ownership, source context, diagnostics and this declaration's quantifiers.
    Registry &registry;
    const Module &module;
    const Reporter &out;
    Scope &scope;
    // Resolve symbolic children bottom-up without recursively expanding aliases.
    Id type(const ast::TypeId &root);
    // Classify a completed symbolic node without executing source code or expanding recursive aliases.
    Id finish(Node node, const ast::NodeSource &source);
    // Route metadata diagnostics through the shared logical/include provenance renderer.
    void diagnostic(const ast::NodeSource &source, std::string message, Severity severity = Severity::error) const;
};

// Check quantified variable use after an entire declaration/overload has been consumed.
void check_scope(const Scope &scope, const Module &module, const ast::NodeSource &source, const Reporter &out);
// Product children add uses; union alternatives take their maximum, with counts saturated at two.
void merge_uses(Uses &target, const Uses &source, bool alternatives);
// Encode exact names without delimiter collisions between modules, declarations or overloads.
std::string scope_identity(const Key &key, std::string_view role, std::size_t variant = 0);
// Collect declarations and exported metadata before resolving any body.
void collect(Registry &registry, std::span<const std::unique_ptr<Module>> modules, const Reporter &out);
// Resolve each specification overload and its constraints in an independent variable scope.
void contracts(Registry &registry, const Reporter &out);
// Preserve exact constant arithmetic in type bounds with a finite allocation budget.
Id constant_node(Resolver &resolver, Node node, const ast::NodeSource &source);
} // namespace erlang_aot::semantic::types
