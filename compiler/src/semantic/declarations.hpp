#pragma once
#include "patterns.hpp"
#include <erlang_aot/abi/immediate_services.hpp>
#include <erlang_aot/compiler/ast/module.hpp>
#include <functional>
#include <map>

namespace erlang_aot::semantic {
// Compare decoded names and arities without host-dependent hashes.
struct FunctionKey {
    // Preserve exact Erlang identity, including quoted and Unicode names.
    std::u32string name;
    std::size_t arity;
    auto operator<=>(const FunctionKey &) const = default;
};

struct BindingId {
    // Source-ordered clause and local definition indices never depend on spelling or addresses.
    std::size_t clause;
    std::size_t local;
    auto operator<=>(const BindingId &) const = default;
};

enum class BindingUse { read, definition, exact_check };
enum class BindingContext { head, guard, body };

struct Binding {
    // Locate each read, definition or exact-equality obligation in the original owned syntax.
    ast::ExprId expression;
    BindingId identity;
    BindingUse use;
    BindingContext context;
};

struct BindingDefinition {
    // Retain declaration spelling/source separately from clause-local identity.
    std::u32string name;
    ast::ExprId expression;
    // Only a whole original argument has a projection fact; extracted/new values remain unknown.
    std::optional<std::size_t> argument;
};

struct ClauseBindings {
    // Stable local slots include head and body definitions, never definitions from another clause.
    std::vector<BindingDefinition> definitions;
};

struct ServiceResolution {
    // A resolved erlang signature is semantic authorization, independent of runtime builtin registrations.
    FunctionKey identity;
    bool guard_legal;
    bool legacy_test;
    // Missing operations denote legal signatures whose numeric/container/process owner is still deferred.
    std::optional<abi::v1::ImmediateOperation> operation;
};

struct Function {
    // Retain declaration identity beside the immutable AST.
    FunctionKey key;
    ast::FormId form;
    // Only explicitly declared exports permit remote calls.
    bool exported = false;
    // Stable private ABI name is independent of addresses and table order.
    std::string symbol;
    // Preserve explicit binding operations and their clause-owned definitions outside syntax.
    std::vector<Binding> bindings = {};
    std::vector<ClauseBindings> clause_bindings = {};
    // Each case expression lists the identities bound by all of its clauses; lowering joins them after the case.
    std::map<const ast::Expression *, std::vector<BindingId>> exports = {};
    // Validated head/body patterns retain source identities for later match planning, without enabling execution.
    std::vector<NormalizedPattern> patterns = {};
    // Only semantically resolved service calls may reach lowering; keys borrow immutable owned syntax.
    std::map<const ast::Expression *, ServiceResolution> services = {};
};

struct RecordLayout {
    // Preserve the source tag independently of runtime atom identities.
    const ast::Atom &name;
    // Distinguish future native representations from ordinary tuple records.
    bool native;
    // Borrow declaration order, source locations and owned default syntax.
    const std::vector<ast::RecordDeclarationField> &fields;
    // Resolve field names without repeatedly scanning wide declarations.
    std::map<std::u32string, std::size_t> positions;
    // Build positions once; duplicate declarations/fields are diagnosed by the module indexer.
    explicit RecordLayout(const ast::RecordDeclaration &declaration);
};

struct Module {
    // Borrow the batch-owned immutable AST for the duration of semantic analysis.
    const ast::Module *syntax = nullptr;
    // Preserve physical input identity even when the AST has no forms.
    std::string file;
    // Decoded module identity is established before function indexing.
    std::u32string name;
    std::optional<ast::FormId> declaration;
    // Source-ordered declarations and deterministic lookup live outside syntax.
    std::vector<Function> functions;
    std::map<FunctionKey, std::size_t> lookup;
    // Borrow source-ordered tuple record declarations and their owned default syntax.
    std::map<std::u32string, RecordLayout> records = {};
    // Origin-table identity preserves declaration-before-use across macro and include boundaries.
    std::map<const ast::TokenOrigin *, std::size_t> source_order = {};
    std::map<std::u32string, std::size_t> record_order = {};
    // Escript sources implicitly export main/1, accept -mode and use escript exit semantics.
    bool escript = false;
};

using Reporter = std::function<void(const Diagnostic &)>;
// Attach logical and physical/macro/include provenance to semantic diagnostics.
void report(const Module &module, const ast::NodeSource *source, std::string message, const Reporter &reporter,
            Severity severity = Severity::error);
// Index declarations and validate the whole module before any lowering occurs.
std::unique_ptr<Module> index(const ast::Module &syntax, std::string file, const Reporter &reporter,
                              bool escript = false);
// Parse an Erlang declaration arity without narrowing arbitrary precision integers.
std::optional<std::size_t> arity(const Integer &value);
} // namespace erlang_aot::semantic
