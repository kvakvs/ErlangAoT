#pragma once
#include <erlang_aot/compiler/ast/module.hpp>
#include <optional>

namespace erlang_aot::ast {
// Internal mutable construction API; consumers receive only the finished Module.
class Builder {
  public:
    class Transaction {
      public:
        // Discard uncommitted nodes without throwing during stack unwinding.
        ~Transaction();
        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;
        // Publish exactly one complete form; destruction otherwise rolls back all allocations.
        void commit(FormId root);

      private:
        friend class Builder;
        // Save arena sizes before opening the single active form transaction.
        explicit Transaction(Builder &builder, std::span<const Token> tokens, const Token &end,
                             FeatureSnapshot features);
        Builder &builder_;
        std::size_t expressions_;
        std::size_t terms_;
        std::size_t types_;
        std::size_t forms_;
        std::size_t patterns_;
        std::size_t origins_;
        // A committed transaction never rolls back its now-published root.
        bool committed_ = false;
    };

    // Start an empty owner with no open transaction.
    Builder() = default;
    Builder(const Builder &) = delete;
    Builder &operator=(const Builder &) = delete;
    // A transaction must not outlive its builder; nested transactions are rejected.
    Transaction begin(std::span<const Token> tokens, const Token &end, FeatureSnapshot features = {});
    // Construct checked extents and typed nodes only inside the active form.
    NodeSource source(std::size_t begin, std::size_t end, std::size_t anchor) const;
    ExprId expression(ExprValue value, NodeSource source) const;
    TermId term(TermValue value, NodeSource source) const;
    TypeId type(TypeValue value, NodeSource source) const;
    // Reclaim temporary attribute expressions after conversion to independently owned literal terms.
    void discard_expressions(std::size_t begin) const;
    FormId form(FormValue value, NodeSource source) const;
    PatternSyntaxId pattern(PatternValue value, NodeSource source) const;
    // Read-only inspection is valid until the next builder mutation.
    const Module &view() const;
    Module finish(FeatureSnapshot features = {}) &&;

  private:
    // Own flat arenas and the origin-table handle of the currently open transaction.
    Module module_;
    std::optional<OriginId> active_;
    // Reject cross-form sources and invalid child handles before node publication.
    void validate(const NodeSource &source) const;
    void validate(const FormValue &value) const;
    void validate(const ExprValue &value) const;
};
} // namespace erlang_aot::ast
