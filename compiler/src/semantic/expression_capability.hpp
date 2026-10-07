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
    // Record declarations determine whether ordinary tuple lowering is authorized.
    const Module &module;

    // Empty reasons denote supported syntax; nonempty reasons identify deferred families.
    std::string_view operator()(const ast::Variable &) const { return {}; }

    std::string_view operator()(const ast::Group &) const { return {}; }

    std::string_view operator()(const ast::IntegerLiteral &) const;
    std::string_view operator()(const ast::CharacterLiteral &) const;
    std::string_view operator()(const ast::UnaryExpression &) const;

    std::string_view operator()(const ast::CallExpression &) const { return {}; }

    std::string_view operator()(const ast::BinaryExpression &) const;

    std::string_view operator()(const ast::Atom &) const { return {}; }

    std::string_view operator()(const ast::FloatLiteral &) const { return {}; }

    std::string_view operator()(const ast::StringLiteral &) const { return {}; }

    std::string_view operator()(const ast::Tuple &) const { return {}; }

    std::string_view operator()(const ast::List &) const { return {}; }

    std::string_view operator()(const ast::Bitstring &) const { return {}; }

    std::string_view operator()(const ast::MatchExpression &) const { return {}; }

    std::string_view operator()(const ast::CatchExpression &) const { return {}; }

    std::string_view operator()(const ast::RemoteExpression &) const { return "dynamic calls"; }

    std::string_view operator()(const ast::MapExpression &) const { return {}; }

    std::string_view operator()(const ast::RecordExpression &) const;

    std::string_view operator()(const ast::RecordAccess &) const;

    std::string_view operator()(const ast::RecordIndex &) const;

    std::string_view operator()(const ast::BlockExpression &) const { return {}; }

    std::string_view operator()(const ast::CaseExpression &) const { return {}; }

    std::string_view operator()(const ast::IfExpression &) const { return {}; }

    std::string_view operator()(const ast::ReceiveExpression &) const { return "receive"; }

    std::string_view operator()(const ast::LocalFunReference &) const;

    std::string_view operator()(const ast::RemoteFunReference &) const;

    std::string_view operator()(const ast::FunExpression &) const { return {}; }

    std::string_view operator()(const ast::TryExpression &) const;

    std::string_view operator()(const ast::MaybeExpression &) const { return {}; }

    std::string_view operator()(const ast::ListComprehension &) const { return {}; }

    std::string_view operator()(const ast::MapComprehension &) const { return {}; }

    std::string_view operator()(const ast::BinaryComprehension &) const { return {}; }
};
} // namespace erlang_aot::semantic
