#pragma once
#include "../declarations.hpp"
#include "syntax.hpp"

namespace erlang_aot::semantic::types {
struct Key {
    // Name/arity is scoped to an exact decoded module identity.
    std::string module;
    std::string name;
    std::size_t arity;
    auto operator<=>(const Key &) const = default;
};

struct Declaration {
    // Borrow immutable declaration provenance; bodies are resolved once, references stay finite.
    const Module *module;
    ast::FormId form;
    Key key;
    ast::TypeDeclarationKind kind;
    std::vector<std::string> parameters;
    bool exported = false;
    std::optional<Id> body;
};

struct Overload {
    // Declared contracts are never implementation facts or runtime guards.
    Id function;
    std::vector<std::pair<std::string, Id>> constraints;
    ast::NodeSource source;
};

struct Contract {
    // Keep the defining source and spec/callback namespace distinct from inferred facts.
    const Module *module;
    ast::FormId form;
    Key key;
    bool callback;
    bool optional = false;
    std::vector<Overload> overloads;
};

struct Record {
    // Borrow the defining form and retain source-ordered field contracts, without evaluating defaults.
    const Module *module;
    ast::FormId form;
    std::vector<std::pair<std::string, Id>> fields;
};

struct Registry {
    // Establish the bounded type owner before collecting any declaration references.
    explicit Registry(Limits limits = {}) : graph(limits) {}

    // One bounded owner retains symbolic definitions and independent contract provenance.
    Graph graph;
    std::vector<Declaration> declarations;
    std::map<Key, std::size_t> lookup;
    std::vector<Contract> contracts;
    std::map<std::pair<std::string, std::string>, Record> records;
    std::map<std::string, const Module *> modules;
    // Cache one-layer alias instantiations by canonical reference and actual argument identities.
    std::map<Id, Id> expansions;
};

// Resolve all metadata within one batch; external metadata is diagnosed unknown.
std::unique_ptr<Registry> resolve_declarations(std::span<const std::unique_ptr<Module>> modules, const Reporter &out,
                                               Limits limits = {});
// Reveal one alias layer with actual substitution; nominal identities and external opaques stay closed.
std::optional<Id> expand_reference(Registry &registry, Id reference, std::string_view requesting_module);
} // namespace erlang_aot::semantic::types
