#pragma once
#include <erlang_aot/compiler/ast/module.hpp>
#include <ostream>

namespace erlang_aot::printing {
// Reuse canonical Erlang spelling for scalar values, including escaped strings.
std::string literal(TokenKind kind, const TokenValue &value);
std::string atom(const ast::Atom &value);

class TreePrinter {
  public:
    // Borrow the immutable module while streaming its tree to the caller's destination.
    TreePrinter(std::ostream &output, const ast::Module &module);
    void run();

    // Exhaustive visitors keep every supported AST alternative visible in the output.
    void operator()(const ast::ModuleAttribute &value);
    void operator()(const ast::FileAttribute &value);
    void operator()(const ast::Function &value);
    void operator()(const ast::ExportAttribute &value);
    void operator()(const ast::ImportAttribute &value);
    void operator()(const ast::ImportRecordAttribute &value);
    void operator()(const ast::GenericAttribute &value);
    void operator()(const ast::RecordDeclaration &value);
    void operator()(const ast::RecordDeclarationField &value);
    void operator()(const ast::DocumentationAttribute &value);
    void operator()(const ast::DocumentationEntry &value);
    void operator()(const ast::TermTuple &value);
    void operator()(const ast::TermList &value);
    void operator()(const ast::TermMap &value);
    void operator()(const ast::TermBits &value);
    void operator()(const ast::TermFunction &value);
    void operator()(const ast::FunctionClause &value);
    void operator()(const ast::GuardSyntax &value);
    void operator()(const ast::GuardConjunction &value);
    void operator()(const ast::RestrictedPattern &value);
    void operator()(const ast::PatternCandidate &value);
    void operator()(const ast::Atom &value);
    void operator()(const ast::Variable &value);
    void operator()(const ast::IntegerLiteral &value);
    void operator()(const ast::FloatLiteral &value);
    void operator()(const ast::CharacterLiteral &value);
    void operator()(const ast::StringLiteral &value);
    void operator()(const ast::Tuple &value);
    void operator()(const ast::List &value);
    void operator()(const ast::Group &value);
    void operator()(const ast::UnaryExpression &value);
    void operator()(const ast::BinaryExpression &value);
    void operator()(const ast::MatchExpression &value);
    void operator()(const ast::CatchExpression &value);
    void operator()(const ast::CallExpression &value);
    void operator()(const ast::RemoteExpression &value);
    void operator()(const ast::MapExpression &value);
    void operator()(const ast::MapField &value);
    void operator()(const ast::RecordExpression &value);
    void operator()(const ast::RecordAccess &value);
    void operator()(const ast::RecordIndex &value);
    void operator()(const ast::RecordField &value);
    void operator()(const ast::Bitstring &value);
    void operator()(const ast::BinarySegment &value);
    void operator()(const ast::BinaryModifier &value);
    void operator()(const ast::BlockExpression &value);
    void operator()(const ast::CaseExpression &value);
    void operator()(const ast::IfExpression &value);
    void operator()(const ast::ReceiveExpression &value);
    void operator()(const ast::BranchClause &value);
    void operator()(const ast::IfClause &value);
    void operator()(const ast::ReceiveTimeout &value);
    void operator()(const ast::LocalFunReference &value);
    void operator()(const ast::RemoteFunReference &value);
    void operator()(const ast::FunExpression &value);
    void operator()(const ast::TryExpression &value);
    void operator()(const ast::CatchClause &value);
    void operator()(const ast::MaybeExpression &value);
    void operator()(const ast::MaybeMatch &value);
    void operator()(const ast::ListComprehension &value);
    void operator()(const ast::MapComprehension &value);
    void operator()(const ast::BinaryComprehension &value);
    void operator()(const ast::Qualifier &value);
    void operator()(const ast::ZippedQualifier &value);
    void operator()(const ast::FilterQualifier &value);
    void operator()(const ast::ListGenerator &value);
    void operator()(const ast::BinaryGenerator &value);
    void operator()(const ast::MapGenerator &value);

  private:
    using Reference = std::variant<
        ast::FormId, ast::ExprId, ast::TermId, const ast::RecordDeclarationField *, const ast::DocumentationEntry *,
        ast::PatternSyntaxId, const ast::FunctionClause *, const ast::GuardSyntax *, const ast::GuardConjunction *,
        const ast::MapField *, const ast::RecordField *, const ast::BinarySegment *, const ast::BinaryModifier *,
        const ast::BranchClause *, const ast::IfClause *, const ast::ReceiveTimeout *, const ast::CatchClause *,
        const ast::MaybeMatch *, const ast::Qualifier *, const ast::ZippedQualifier *>;

    struct Work {
        // Retain child roles and depth independently of the native call stack.
        std::string role;
        std::size_t depth;
        Reference reference;
    };

    // Output and borrowed storage remain valid throughout the iterative traversal.
    std::ostream &output_;
    const ast::Module &module_;
    // Children are scheduled in source order, then reversed for depth-first traversal.
    std::vector<Work> pending_;
    std::size_t depth_ = 0;

    // Enqueue a direct child without recursively visiting it.
    void child(std::string role, Reference reference);
    void optional_child(std::string role, const std::optional<ast::ExprId> &reference);
    // Schedule either a body expression handle or an embedded conditional match.
    void maybe_child(std::string role, const ast::ExprId &value);
    void maybe_child(std::string role, const ast::MaybeMatch &value);
    // Queue qualifier groups without flattening zipped versus sequential structure.
    void qualifier_child(std::string role, const ast::Qualifier &value);
    void qualifier_child(std::string role, const ast::ZippedQualifier &value);
    void qualifier_children(const std::vector<ast::ComprehensionQualifier> &values);
    // Write one object's role and indentation; its visitor supplies the scalar fields.
    void prefix(const Work &work);
    // Dispatch arena handles through the module's ownership-checked accessors.
    void visit(const ast::FormId &id);
    void visit(const ast::ExprId &id);
    void visit(const ast::TermId &id);
    // Render arities as compact scalar entries while retaining declaration order.
    void arities(const std::vector<ast::NameArity> &values);
    void visit(const ast::PatternSyntaxId &id);

    template <typename T> void visit(const T *value) { (*this)(*value); }

    // Preserve positional roles for ordered arena handles and embedded objects.
    template <typename Range> void handles(std::string_view role, const Range &values) {
        std::size_t index = 0;
        for (const auto &id : values) {
            child(std::string(role) + '[' + std::to_string(index++) + ']', id);
        }
    }

    template <typename T> void objects(std::string_view role, const std::vector<T> &values) {
        std::size_t index = 0;
        for (const auto &value : values) {
            child(std::string(role) + '[' + std::to_string(index++) + ']', &value);
        }
    }
};
} // namespace erlang_aot::printing
