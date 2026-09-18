#pragma once
#include "value.hpp"
#include <functional>

namespace erlang_aot {
enum class ExprKind : std::uint8_t {
    literal,
    variable,
    unary,
    binary,
    call,
    tuple,
    list,
    map,
    map_update,
    bits,
    segment,
    record,
    external_fun
};

struct Expr {
    // Retain operators and source locations separately from evaluated values.
    ExprKind kind;
    Token token;
    std::vector<Expr> children;
    // Binary segment modifiers (type/endian/signed/unit) stay local to this grammar.
    std::vector<Token> modifiers;
};

class ExpressionParser {
  public:
    // Parse project tokens directly, avoiding lossy re-lexing after expansion.
    ExpressionParser(std::span<const Token> tokens, std::size_t maximum_depth);
    Expr parse();

  private:
    // Bound recursion and preserve an EOF diagnostic anchor.
    std::span<const Token> input_;
    Token anchor_;
    std::size_t depth_ = 0;
    std::size_t maximum_depth_;
    // Implement precedence climbing and recursive term/container envelopes.
    Expr expression(int minimum = 0);
    Expr primary();
    Expr scalar();
    Expr sigil();
    Expr record(std::optional<Expr> base = {});
    Expr external_fun();
    Expr postfix(Expr base);
    Expr collection(ExprKind kind, std::u32string_view closing);
    Expr map(ExprKind kind, std::optional<Expr> base = {});
    Expr binary_literal();
    Expr segment();
    Expr atom_or_call(Token name);
    bool take(std::u32string_view symbol);
    void expect(std::u32string_view symbol);
    Token consume();
};

// Validate every branch before evaluation, including short-circuited expressions.
void validate_guard(const Expr &expression);
bool literal_term(const Expr &expression);
Value evaluate(const Expr &expression, const std::function<bool(std::u32string_view)> &defined);
Value evaluate_operator(std::u32string_view name, const std::vector<Value> &arguments);
Value guard_call(std::u32string_view name, const std::vector<Value> &arguments);
Value evaluate_bits(const Expr &expression, const std::function<bool(std::u32string_view)> &defined);
bool operator_signature(std::u32string_view name, std::size_t arity);
bool guard_signature(std::u32string_view name, std::size_t arity);
// A condition is true only for atom true; ordinary evaluation errors mean false.
bool condition(std::span<const Token> input, std::size_t depth,
               const std::function<bool(std::u32string_view)> &defined);
Value parse_term(std::span<const Token> input, std::size_t depth);
} // namespace erlang_aot
