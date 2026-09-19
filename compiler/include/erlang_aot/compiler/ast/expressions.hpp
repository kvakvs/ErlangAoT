#pragma once
#include <erlang_aot/compiler/ast/clauses.hpp>
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

struct BinaryModifier {
    // Keep unknown/duplicate atom modifiers and optional integer parameters unevaluated.
    Atom name;
    std::optional<Integer> parameter;
    NodeSource source;
};

struct BinarySegment {
    // Omitted size/types differ from explicit values or a nonempty ordered modifier list.
    ExprId value;
    std::optional<ExprId> size;
    std::optional<std::vector<BinaryModifier>> modifiers;
    NodeSource source;
};

struct Bitstring {
    // Preserve segment syntax without imposing a runtime byte layout or evaluating values.
    std::vector<BinarySegment> segments;
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

struct BranchClause {
    // OTP accepts an expression candidate here; semantic pattern checks remain deferred.
    PatternSyntaxId pattern;
    std::optional<GuardSyntax> guard;
    std::vector<ExprId> body;
    NodeSource source;
};

struct IfClause {
    // Guard-only branches keep their nonempty alternatives and body separate.
    GuardSyntax guard;
    std::vector<ExprId> body;
    NodeSource source;
};

struct BlockExpression {
    // Retain the begin/end boundary around a nonempty expression sequence.
    std::vector<ExprId> body;
};

struct CaseExpression {
    // Keep the scrutinee and ordered candidate branches without lowering matches.
    ExprId value;
    std::vector<BranchClause> clauses;
};

struct IfExpression {
    // Each branch begins with a guard rather than a pattern.
    std::vector<IfClause> clauses;
};

struct ReceiveTimeout {
    // An explicit after part owns its timeout expression and nonempty body.
    ExprId timeout;
    std::vector<ExprId> body;
    NodeSource source;
};

struct ReceiveExpression {
    // An after-only receive has no clauses; at least one branch or timeout is required.
    std::vector<BranchClause> clauses;
    std::optional<ReceiveTimeout> after;
};

struct LocalFunReference {
    // Local references require an atom name and literal arity.
    Atom name;
    Integer arity;
};

struct RemoteFunReference {
    // Remote names and arity can be dynamic without admitting arbitrary expressions.
    std::variant<Atom, Variable> module;
    std::variant<Atom, Variable> name;
    std::variant<Integer, Variable> arity;
};

struct FunExpression {
    // Absence of a recursive name distinguishes anonymous fun clauses.
    std::optional<Variable> name;
    std::vector<FunctionClause> clauses;
};

struct CatchClause {
    // Omitted class/stacktrace project to throw/_; reason syntax is restricted pat_expr.
    std::optional<std::variant<Atom, Variable>> exception_class;
    PatternSyntaxId reason;
    std::optional<Variable> stacktrace;
    std::optional<GuardSyntax> guard;
    std::vector<ExprId> body;
    NodeSource source;
};

struct TryExpression {
    // Preserve optional of/catch/after parts; at least catch or after is required.
    std::vector<ExprId> body;
    std::optional<std::vector<BranchClause>> of;
    std::optional<std::vector<CatchClause>> handlers;
    std::optional<std::vector<ExprId>> after;
};

struct MaybeMatch {
    // Conditional matches are confined to maybe bodies and defer pattern legality.
    PatternSyntaxId pattern;
    ExprId value;
    NodeSource source;
};

struct MaybeExpression {
    // Keep expression and conditional-match order and optional nonempty else clauses.
    std::vector<std::variant<ExprId, MaybeMatch>> body;
    std::optional<std::vector<BranchClause>> otherwise;
};

struct FilterQualifier {
    // Match expressions remain filters until later compr_assign validation/lowering.
    ExprId expression;
};

struct ListGenerator {
    // Preserve permissive pattern syntax, source collection and strict arrow spelling.
    PatternSyntaxId pattern;
    ExprId input;
    bool strict;
};

struct BinaryGenerator {
    // Binary syntax is checked while parsing; segment/pattern legality remains deferred.
    PatternSyntaxId pattern;
    ExprId input;
    bool strict;
};

struct MapGenerator {
    // Exact key/value candidate syntax is distinct from list and binary generators.
    PatternSyntaxId key;
    PatternSyntaxId value;
    ExprId input;
    bool strict;
};

struct Qualifier {
    // A simple qualifier owns its full token extent and operator anchor.
    std::variant<FilterQualifier, ListGenerator, BinaryGenerator, MapGenerator> value;
    NodeSource source;
};

struct ZippedQualifier {
    // Zip groups contain at least two simple qualifiers, including grammar-permitted filters.
    std::vector<Qualifier> qualifiers;
    NodeSource source;
};

using ComprehensionQualifier = std::variant<Qualifier, ZippedQualifier>;

struct ListComprehension {
    // OTP 29 admits multiple ordered list templates.
    std::vector<ExprId> templates;
    std::vector<ComprehensionQualifier> qualifiers;
};

struct MapComprehension {
    // Keep template field operators and order, including multiple templates.
    std::vector<MapField> templates;
    std::vector<ComprehensionQualifier> qualifiers;
};

struct BinaryComprehension {
    // The binary template uses expr_max syntax and is not necessarily a bitstring literal.
    ExprId expression;
    std::vector<ComprehensionQualifier> qualifiers;
};

using ExprValue =
    std::variant<Atom, Variable, IntegerLiteral, FloatLiteral, CharacterLiteral, StringLiteral, Tuple, List, Group,
                 Bitstring, UnaryExpression, BinaryExpression, MatchExpression, CatchExpression, CallExpression,
                 RemoteExpression, MapExpression, RecordExpression, RecordAccess, RecordIndex, BlockExpression,
                 CaseExpression, IfExpression, ReceiveExpression, LocalFunReference, RemoteFunReference, FunExpression,
                 TryExpression, MaybeExpression, ListComprehension, MapComprehension, BinaryComprehension>;

struct Expression {
    // Associate a closed, typed payload with its expanded-token extent.
    ExprValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
