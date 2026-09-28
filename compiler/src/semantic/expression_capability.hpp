#pragma once
#include "capabilities.hpp"

namespace erlang_aot::semantic {
// Closed overloads force each newly added expression family to choose a capability policy.
struct ExpressionCapability {
    // Borrow immutable syntax to distinguish literal call targets from dynamic expressions.
    const ast::Module &syntax;
    // Retain the expression identity and target width for checked literal decoding.
    ast::ExprId id;
    unsigned word_bits;

    // Empty reasons denote supported syntax; nonempty reasons identify deferred families.
    std::string_view operator()(const ast::Variable &) const { return {}; }

    std::string_view operator()(const ast::Group &) const { return {}; }

    std::string_view operator()(const ast::IntegerLiteral &) const;
    std::string_view operator()(const ast::CharacterLiteral &) const;
    std::string_view operator()(const ast::UnaryExpression &) const;
    std::string_view operator()(const ast::CallExpression &) const;
    std::string_view operator()(const ast::BinaryExpression &) const;

    std::string_view operator()(const ast::Atom &) const { return "atom expressions"; }

    std::string_view operator()(const ast::FloatLiteral &) const { return "heap expressions"; }

    std::string_view operator()(const ast::StringLiteral &) const { return "heap expressions"; }

    std::string_view operator()(const ast::Tuple &) const { return "heap expressions"; }

    std::string_view operator()(const ast::List &) const { return "heap expressions"; }

    std::string_view operator()(const ast::Bitstring &) const { return "heap expressions"; }

    std::string_view operator()(const ast::MatchExpression &) const { return "pattern matching"; }

    std::string_view operator()(const ast::CatchExpression &) const { return "exceptions"; }

    std::string_view operator()(const ast::RemoteExpression &) const { return "dynamic calls"; }

    std::string_view operator()(const ast::MapExpression &) const { return "heap expressions"; }

    std::string_view operator()(const ast::RecordExpression &) const { return "heap expressions"; }

    std::string_view operator()(const ast::RecordAccess &) const { return "heap expressions"; }

    std::string_view operator()(const ast::RecordIndex &) const { return "heap expressions"; }

    std::string_view operator()(const ast::BlockExpression &) const { return "pattern matching"; }

    std::string_view operator()(const ast::CaseExpression &) const { return "pattern matching"; }

    std::string_view operator()(const ast::IfExpression &) const { return "guards"; }

    std::string_view operator()(const ast::ReceiveExpression &) const { return "receive"; }

    std::string_view operator()(const ast::LocalFunReference &) const { return "closures"; }

    std::string_view operator()(const ast::RemoteFunReference &) const { return "closures"; }

    std::string_view operator()(const ast::FunExpression &) const { return "closures"; }

    std::string_view operator()(const ast::TryExpression &) const { return "exceptions"; }

    std::string_view operator()(const ast::MaybeExpression &) const { return "pattern matching"; }

    std::string_view operator()(const ast::ListComprehension &) const { return "heap expressions"; }

    std::string_view operator()(const ast::MapComprehension &) const { return "heap expressions"; }

    std::string_view operator()(const ast::BinaryComprehension &) const { return "heap expressions"; }
};
} // namespace erlang_aot::semantic
