#pragma once
#include "binary_options.hpp"
#include "declarations.hpp"

namespace erlang_aot::semantic {
enum class MatchOperation : std::uint8_t {
    bind,
    exact_binding,
    exact_literal,
    tuple_shape,
    tuple_element,
    cons_shape,
    cons_head,
    cons_tail,
    map_shape,
    map_lookup,
    binary_start,
    binary_extract,
    binary_finish,
    // Native records: test the captured identity (check in index, module in record_module, name in literal), then
    // extract one field (name in literal) or fail to match.
    record_test,
    record_field,
    success,
    mismatch
};
enum class EmptyValue : std::uint8_t { tuple, list };
using MatchLiteral = std::variant<std::int64_t, ast::Atom, EmptyValue, ast::IntegerLiteral, ast::FloatLiteral>;

struct MatchNode {
    // Retain source and candidate input slot, with identities independent of spelling.
    ast::ExprId source;
    MatchOperation operation;
    std::size_t input;
    std::optional<BindingId> binding = {};
    std::optional<MatchLiteral> literal = {};
    // Each test has explicit success/mismatch continuations; terminals do not consume these edges.
    std::size_t success = 0;
    std::size_t mismatch = 0;
    // Checked extraction writes a distinct candidate slot; index is a tuple arity or zero-based field position.
    std::size_t output = 0;
    std::size_t index = 0;
    // Embedded map keys retain their analyzed incoming read bindings and failure continuation.
    std::optional<ast::ExprId> key = {};
    // Binary extraction retains canonical modifiers and an explicit next-cursor candidate slot.
    std::optional<BinaryOptions> bits = {};
    std::size_t cursor_output = 0;
    // The module a native record test names.
    std::optional<ast::Atom> record_module = {};
};

struct MatchPlan {
    // Candidate inputs are original arguments; bindings are tentative until the caller's success edge.
    std::size_t inputs;
    std::vector<BindingId> outputs;
    // Flat tests and checked extractions share explicit mismatch continuations in both source contexts.
    std::vector<MatchNode> nodes;
    // Include original inputs and every checked extracted value, independently of source binding identities.
    std::size_t values = 0;
    // A binary generator's plan leaves the bits after the element in this value.
    std::optional<std::size_t> rest = {};
};

// How a binary generator's pattern reads its input (OTP v3_core): `element` matches a prefix and keeps the rest;
// `skip` also ignores segment values and repeated names and reads floats as integers, to step over a rejected
// element.
enum class GeneratorPattern : std::uint8_t { none, element, skip };

struct MatchOptions {
    // Select one source clause and target word layout within a shared construction ceiling.
    std::size_t clause = 0;
    unsigned word_bits = 64;
    std::size_t work_limit = 100'000;
    GeneratorPattern generator = GeneratorPattern::none;
};

// Build a one-input body pattern with existing bindings preserved as exact constraints.
std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const ast::ExprId &pattern,
                                         const Reporter &out, MatchOptions options = {});
// Consume normalized semantics and binding events within explicit node/work ceilings.
std::optional<MatchPlan> make_match_plan(const Module &module, const Function &function, const Reporter &out,
                                         MatchOptions options = {});
} // namespace erlang_aot::semantic
