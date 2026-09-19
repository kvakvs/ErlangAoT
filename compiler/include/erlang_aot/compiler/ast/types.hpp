#pragma once
#include <erlang_aot/compiler/ast/expressions.hpp>

namespace erlang_aot::ast {
struct TypeGroup {
    // Parentheses retain source boundaries without changing type meaning.
    TypeId type;
};

struct AnnotatedType {
    // Variable binding belongs to later analysis.
    Variable variable;
    TypeId type;
};

struct UnionType {
    // Preserve union order and explicit grouping through typed children.
    TypeId left;
    TypeId right;
};

struct RangeType {
    // Endpoints are syntax, not evaluated integer bounds.
    TypeId first;
    TypeId last;
};

struct UnaryType {
    // Share closed operator identities without expression evaluation.
    UnaryOperator operation;
    TypeId operand;
};

struct BinaryTypeOperator {
    // Type arithmetic retains ordered operands independently of expression nodes.
    BinaryOperator operation;
    TypeId left;
    TypeId right;
};

struct TypeApplication {
    // Remote modules suppress predefined classification; local names remain unresolved.
    std::optional<Atom> module;
    Atom name;
    bool predefined;
    std::vector<TypeId> arguments;
};

struct TupleType {
    // any distinguishes tuple() from the exact empty tuple {}.
    bool any;
    std::vector<TypeId> elements;
};

struct ListType {
    // Absent element denotes []; nonempty denotes [T,...].
    std::optional<TypeId> element;
    bool nonempty;
};

struct MapTypeField {
    // Association and exact fields retain distinct type grammar roles.
    MapFieldKind kind;
    TypeId key;
    TypeId value;
    NodeSource source;
};

struct MapType {
    // any distinguishes map() from the exact empty map #{}.
    bool any;
    std::vector<MapTypeField> fields;
};

struct RecordTypeField {
    // Record field refinements retain unresolved names and type-only children.
    Atom name;
    TypeId type;
    NodeSource source;
};

struct RecordType {
    // Qualified native records retain original module and record names.
    std::optional<Atom> module;
    Atom name;
    std::vector<RecordTypeField> fields;
};

struct BitstringType {
    // Omitted base/unit normalize to zero; explicit values retain their syntax.
    std::optional<TypeId> base;
    std::optional<TypeId> unit;
};

struct FunType {
    // No result denotes fun(); absent arguments with a result denote (...)->T.
    std::optional<std::vector<TypeId>> arguments;
    std::optional<TypeId> result;
};

using TypeValue = std::variant<Atom, Variable, IntegerLiteral, CharacterLiteral, TypeGroup, AnnotatedType, UnionType,
                               RangeType, UnaryType, BinaryTypeOperator, TypeApplication, TupleType, ListType, MapType,
                               RecordType, BitstringType, FunType>;

struct TypeSyntax {
    // Types use the same checked ownership and source model as expressions.
    TypeValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
