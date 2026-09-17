#pragma once
#include <erlang_aot/compiler/directive.hpp>
#include <map>

namespace erlang_aot {
struct MacroKey {
    // Keep object form distinct from every parameter arity, including zero.
    std::u32string name;
    std::optional<std::size_t> arity;
    auto operator<=>(const MacroKey&) const = default;
};
struct ConditionalFrame {
    // Reserve branch state and its opener for subsequent conditional processing.
    Span opening;
    DirectiveKind family;
    bool parent_active = true;
    bool branch_selected = false;
    bool seen_else = false;
};
struct FeatureState {
    // Hold per-module overrides and whether feature changes remain legal.
    std::map<std::u32string, bool> overrides;
    bool in_prefix = true;
};
struct ContextState {
    // Resolve contextual macros later, independently from token physical origins.
    std::optional<std::u32string> module;
    std::optional<std::pair<std::u32string, std::size_t>> function;
};
struct OrdinaryForm {
    // Pass original tokens unchanged to subsequent expansion/parsing stages.
    std::vector<Token> tokens;
};
using PreprocessorEvent = std::variant<OrdinaryForm, Directive, Diagnostic>;

class PreprocessorSession {
public:
    // Start an isolated module; this foundation parses syntax without applying directives.
    explicit PreprocessorSession(SourcePtr source);
    // Return one form/directive/error, or EOF; errors never reset failure status.
    std::optional<PreprocessorEvent> next();
    bool failed() const;
private:
    struct IncludeFrame {
        // Keep each physical source and its incremental scanner alive across forms.
        SourcePtr source;
        Lexer lexer;
    };
    // Each module owns its future semantic state; no process-global definitions.
    std::map<MacroKey, Definition> macros_;
    std::vector<IncludeFrame> includes_;
    std::vector<ConditionalFrame> conditionals_;
    FeatureState features_;
    ContextState context_;
    // Latch errors independently of the event stream consumed by the caller.
    bool failed_ = false;

    // Classify a complete lexical form and report misplaced directive syntax.
    PreprocessorEvent classify(std::vector<Token> tokens);
    // Latch failure before returning any diagnostic event.
    PreprocessorEvent error(Diagnostic diagnostic);
};
} // namespace erlang_aot
