#pragma once
#include "preprocessor/expression.hpp"
#include <erlang_aot/compiler/ast/module.hpp>

namespace erlang_aot {
// Normalize only erl_parse literal syntax; ordinary operators and calls are rejected.
class TermNormalizer {
  public:
    explicit TermNormalizer(const ast::Module &module) : module_(module) {}

    Value read(const ast::ExprId &id, bool farity = true) const;
    Value operator()(const ast::Atom &value) const;
    Value operator()(const ast::IntegerLiteral &value) const;
    Value operator()(const ast::FloatLiteral &value) const;
    Value operator()(const ast::CharacterLiteral &value) const;
    Value operator()(const ast::StringLiteral &value) const;
    Value operator()(const ast::Group &value) const;
    Value operator()(const ast::UnaryExpression &value) const;
    Value operator()(const ast::BinaryExpression &value) const;
    Value operator()(const ast::Tuple &value) const;
    Value operator()(const ast::List &value) const;
    Value operator()(const ast::MapExpression &value) const;
    Value operator()(const ast::Bitstring &value) const;
    Value operator()(const ast::RemoteFunReference &value) const;

    // The literal grammar is a strict subset of expressions, closed by rejection.
    template <typename T> Value operator()(const T &) const { throw EvaluationFailure(); }

  private:
    // Each recursive visitor retains its own farity context; map keys disable normalization.
    const ast::Module &module_;
    bool farity_ = true;
    Value child(const ast::ExprId &id) const;
    void segment(Value &output, const ast::BinarySegment &value) const;
};
} // namespace erlang_aot
