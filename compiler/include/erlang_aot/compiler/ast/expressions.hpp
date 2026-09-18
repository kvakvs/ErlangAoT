#pragma once
#include <erlang_aot/compiler/ast/operators.hpp>
#include <erlang_aot/compiler/ast/source.hpp>
#include <erlang_aot/compiler/lexer.hpp>
#include <optional>

namespace erlang_aot::ast {
struct Atom {
    // Store the decoded atom name without depending on runtime atom tables.
    std::u32string name;
};

struct Variable {
    // Preserve spelling; binding and wildcard interpretation belong to later analysis.
    std::u32string name;
};

struct IntegerLiteral {
    // Reuse arbitrary-precision canonical digits from the existing lexer.
    Integer value;
};

struct FloatLiteral {
    // Preserve the scanner's binary64 value without decimal conversion.
    double value;
};

struct CharacterLiteral {
    // Keep character syntax distinct from an integer literal of the same value.
    char32_t value;
};

struct StringLiteral {
    // Retain decoded Unicode characters, independently of original literal spelling.
    std::u32string value;
};

struct Tuple {
    // Keep element order and distinguish tuples from lists, including empty aggregates.
    std::vector<ExprId> elements;
};

struct List {
    // Flatten the comma spine; an absent tail denotes the implicit empty list.
    std::vector<ExprId> elements;
    std::optional<ExprId> tail;
};

struct Group {
    // Retain parentheses for source extents and syntactic precedence boundaries.
    ExprId expression;
};

struct BinarySigilLiteral {
    // Decoded Unicode content denotes UTF-8 bytes, without eager runtime allocation.
    std::u32string value;
};

struct UnaryExpression {
    // Preserve the operator rather than folding constants during parsing.
    UnaryOperator operation;
    ExprId operand;
};

struct BinaryExpression {
    // Operator identity and ordered operands retain associativity in the tree.
    BinaryOperator operation;
    ExprId left;
    ExprId right;
};

struct MatchExpression {
    // The left side is expression syntax; pattern legality is a later check.
    ExprId left;
    ExprId right;
};

struct CatchExpression {
    // Catch binds below every infix operator, independently of later exception lowering.
    ExprId expression;
};

struct CallExpression {
    // Keep general callable expressions, including dynamic and chained calls.
    ExprId target;
    std::vector<ExprId> arguments;
};

struct RemoteExpression {
    // Erlang parsing permits general module/function expressions, even without a call.
    ExprId module;
    ExprId function;
};

enum class MapFieldKind : std::uint8_t { associate, exact };

struct MapField {
    // Preserve field order and operator spelling without checking map-key semantics.
    MapFieldKind kind;
    ExprId key;
    ExprId value;
    NodeSource source;
};

struct MapExpression {
    // An absent base constructs a map; a present base updates it without evaluation.
    std::optional<ExprId> base;
    std::vector<MapField> fields;
};

struct UnresolvedRecordName {
    // Local spelling alone cannot distinguish tuple records from native records.
    Atom name;
};

struct QualifiedRecordName {
    // Explicit module qualification identifies native record syntax.
    Atom module;
    Atom name;
};

struct InferredRecordName {};

struct RecordIdentity {
    // Preserve local, qualified and inferred identities until declaration analysis.
    std::variant<UnresolvedRecordName, QualifiedRecordName, InferredRecordName> value;
    NodeSource source;
};

struct RecordField {
    // Variable names, including wildcard _, remain syntax rather than resolved fields.
    std::variant<Atom, Variable> name;
    ExprId value;
    NodeSource source;
};

struct RecordExpression {
    // Retain construction/update syntax and explicit field assignments in source order.
    std::optional<ExprId> base;
    RecordIdentity identity;
    std::vector<RecordField> fields;
};

struct RecordAccess {
    // Keep field access separate from updates and defer record-layout resolution.
    ExprId base;
    RecordIdentity identity;
    Atom field;
    NodeSource field_source;
};

struct RecordIndex {
    // The index production accepts only atom record/field names and has no base value.
    Atom record;
    Atom field;
    NodeSource name_source;
    NodeSource field_source;
};

using ExprValue =
    std::variant<Atom, Variable, IntegerLiteral, FloatLiteral, CharacterLiteral, StringLiteral, Tuple, List, Group,
                 BinarySigilLiteral, UnaryExpression, BinaryExpression, MatchExpression, CatchExpression,
                 CallExpression, RemoteExpression, MapExpression, RecordExpression, RecordAccess, RecordIndex>;

struct Expression {
    // Associate a closed, typed payload with its expanded-token extent.
    ExprValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
