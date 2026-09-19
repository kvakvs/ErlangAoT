#pragma once
#include <erlang_aot/compiler/ast/module.hpp>
#include <erlang_aot/compiler/preprocessor.hpp>

namespace erlang_aot {
struct ParserLimits {
    // Bound retained origins, total input work, and AST nodes independently of language validity.
    std::size_t form_tokens = 1000000;
    std::size_t total_tokens = 4000000;
    std::size_t nodes = 1000000;
    // One additional reserved diagnostic reports exhaustion of this ordinary-message budget.
    std::size_t diagnostics = 1000;
    // Bound recursive grammar calls; requests above the hard safety ceiling of 512 are clamped.
    std::size_t nesting = 256;
    // Account for tokens, grammar entries, node creation and literal-normalization work.
    std::size_t work = 16000000;
};

struct ParseResult {
    // Retain only completed forms; failed transactions publish no ordinary AST nodes.
    ast::Module module;
    std::vector<Diagnostic> diagnostics;
    // Failure remains latched even when recovery produces later valid forms.
    bool failed;

    // Syntax success does not imply valid bindings, types, or backend support.
    bool succeeded() const { return !failed; }
};

class ParserSession {
  public:
    // Start one independent module parser with bounded work and storage.
    explicit ParserSession(ParserLimits limits = {});
    ~ParserSession();
    ParserSession(const ParserSession &) = delete;
    ParserSession &operator=(const ParserSession &) = delete;
    // Parse exactly one expanded form with an explicit EOF anchor, including empty input.
    void parse_form(std::span<const Token> tokens, const Token &end, FeatureSnapshot features = {});
    // Consume semantic preprocessor events; unexpected directives are contract errors.
    void consume(const PreprocessorEvent &event);
    // A resource limit stops further input work; ordinary syntax errors permit recovery.
    bool stopped() const;
    // Finish once, retaining final feature state separately from per-form snapshots.
    ParseResult finish(FeatureSnapshot features = {}) &&;

  private:
    struct State;
    // Hide mutable AST construction and grammar implementation from consumers.
    std::unique_ptr<State> state_;
};

// Drain a semantic preprocessing session through EOF, unless a parser resource limit stops it.
ParseResult parse_module(PreprocessorSession &preprocessor, ParserLimits limits = {});
} // namespace erlang_aot
