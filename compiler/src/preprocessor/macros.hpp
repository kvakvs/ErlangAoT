#pragma once
#include <deque>
#include <erlang_aot/compiler/preprocessor.hpp>

namespace erlang_aot {
struct Arguments {
    // Preserve raw arguments for substitution/stringification and consumed token count.
    std::vector<std::vector<Token>> values;
    std::size_t consumed = 0;
};

// Collect balanced macro/header arguments, including Erlang blocks and fun types.
Arguments collect_arguments(std::span<const Token> input, const Token &call);

class MacroTable {
  public:
    // Retain definitions and reserved builtin names, including undefined contexts.
    std::map<MacroKey, Definition> definitions;
    std::set<std::u32string> reserved;
    std::set<std::u32string> undefined;
    // Validate a definition before publishing it, or remove every overload of a name.
    void define(Definition definition);
    // Traverse static dependencies, including references in unused arguments.
    void check_cycles(const Definition &definition, const Token &call, std::size_t maximum_depth) const;
    void undefine(std::u32string_view name);
    bool contains(std::u32string_view name, bool include_undefined = false) const;
    // Match OTP's object-only fallback and overloaded arity dispatch.
    const Definition *lookup(const Token &name, std::optional<std::size_t> arity) const;
};

class MacroExpander {
  public:
    // Supply contextual builtins without freezing their values at definition time.
    using Builtin = std::function<std::optional<std::vector<Token>>(const Token &)>;
    MacroExpander(const MacroTable &table, const PreprocessorLimits &limits, Builtin builtin);
    std::vector<Token> expand(std::span<const Token> input);

  private:
    // Borrow session state only for the duration of one form expansion.
    const MacroTable &table_;
    const PreprocessorLimits &limits_;
    Builtin builtin_;
    std::size_t produced_ = 0;
    // Expand one reference, preserving argument annotations and definition traces.
    std::vector<Token> invoke(const Token &name, const Arguments &args, std::deque<Token> &pending);
    std::vector<Token> reference(std::deque<Token> &pending);
    std::vector<Token> substitute(const Definition &definition, const Arguments &arguments, const Token &call);
    std::vector<Token> rescan(std::span<const Token> input);
    void budget(std::size_t count, const Token &call);
};
} // namespace erlang_aot
