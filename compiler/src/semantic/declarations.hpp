#pragma once
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

struct Binding {
    // Associate an immutable variable read with its original argument-array position.
    ast::ExprId expression;
    std::size_t argument;
};

struct Function {
    // Retain declaration identity beside the immutable AST.
    FunctionKey key;
    ast::FormId form;
    // Only explicitly declared exports permit remote calls.
    bool exported = false;
    // Stable private ABI name is independent of addresses and table order.
    std::string symbol;
    // Preserve source-order parameter reads independently of syntax ownership.
    std::vector<Binding> bindings = {};
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
};

using Reporter = std::function<void(const Diagnostic &)>;
// Attach logical and physical/macro/include provenance to semantic diagnostics.
void report(const Module &module, const ast::NodeSource *source, std::string message, const Reporter &reporter,
            Severity severity = Severity::error);
// Index declarations and validate the whole module before any lowering occurs.
std::unique_ptr<Module> index(const ast::Module &syntax, std::string file, const Reporter &reporter);
// Parse an Erlang declaration arity without narrowing arbitrary precision integers.
std::optional<std::size_t> arity(const Integer &value);
} // namespace erlang_aot::semantic
