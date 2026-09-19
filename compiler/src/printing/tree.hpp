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
    TreePrinter(std::ostream &output, const ast::Module &module, std::size_t visits);
    void run();

    // Exhaustive visitors keep every supported AST alternative visible in the output.
    void operator()(const ast::ModuleAttribute &value) const;
    void operator()(const ast::FileAttribute &value) const;
    void operator()(const ast::Function &value);
    void operator()(const ast::Specification &value);
    void operator()(const ast::SpecificationSignature &value);
    void operator()(const ast::TypeConstraint &value);
    void operator()(const ast::TypeGroup &value);
    void operator()(const ast::AnnotatedType &value);
    void operator()(const ast::UnionType &value);
    void operator()(const ast::RangeType &value);
    void operator()(const ast::UnaryType &value);
    void operator()(const ast::BinaryTypeOperator &value);
    void operator()(const ast::TypeApplication &value);
    void operator()(const ast::TupleType &value);
    void operator()(const ast::ListType &value);
    void operator()(const ast::MapType &value);
    void operator()(const ast::RecordType &value);
    void operator()(const ast::BitstringType &value);
    void operator()(const ast::FunType &value);
    void operator()(const ast::TypeDeclaration &value);
    void operator()(const ast::MapTypeField &value);
    void operator()(const ast::RecordTypeField &value);
    void operator()(const ast::ExportAttribute &value) const;
    void operator()(const ast::ImportAttribute &value) const;
    void operator()(const ast::ImportRecordAttribute &value) const;
    void operator()(const ast::GenericAttribute &value);
    void operator()(const ast::RecordDeclaration &value);
    void operator()(const ast::RecordDeclarationField &value);
    void operator()(const ast::DocumentationAttribute &value);
    void operator()(const ast::DocumentationEntry &value);
    void operator()(const ast::TermTuple &value);
    void operator()(const ast::TermList &value);
    void operator()(const ast::TermMap &value);
    void operator()(const ast::TermBits &value) const;
    void operator()(const ast::TermFunction &value) const;
    void operator()(const ast::FunctionClause &value);
    void operator()(const ast::GuardSyntax &value);
    void operator()(const ast::GuardConjunction &value);
    void operator()(const ast::RestrictedPattern &value);
    void operator()(const ast::PatternCandidate &value);
    void operator()(const ast::Atom &value) const;
    void operator()(const ast::Variable &value) const;
    void operator()(const ast::IntegerLiteral &value) const;
    void operator()(const ast::FloatLiteral &value) const;
    void operator()(const ast::CharacterLiteral &value) const;
    void operator()(const ast::StringLiteral &value) const;
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
    void operator()(const ast::RecordIndex &value) const;
    void operator()(const ast::RecordField &value);
    void operator()(const ast::Bitstring &value);
    void operator()(const ast::BinarySegment &value);
    void operator()(const ast::BinaryModifier &value) const;
    void operator()(const ast::BlockExpression &value);
    void operator()(const ast::CaseExpression &value);
    void operator()(const ast::IfExpression &value);
    void operator()(const ast::ReceiveExpression &value);
    void operator()(const ast::BranchClause &value);
    void operator()(const ast::IfClause &value);
    void operator()(const ast::ReceiveTimeout &value);
    void operator()(const ast::LocalFunReference &value) const;
    void operator()(const ast::RemoteFunReference &value) const;
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
    using Reference =
        std::variant<const ast::SpecificationSignature *, const ast::TypeConstraint *, const ast::FunType *,
                     ast::TypeId, const ast::MapTypeField *, const ast::RecordTypeField *, ast::FormId, ast::ExprId,
                     ast::TermId, const ast::RecordDeclarationField *, const ast::DocumentationEntry *,
                     ast::PatternSyntaxId, const ast::FunctionClause *, const ast::GuardSyntax *,
                     const ast::GuardConjunction *, const ast::MapField *, const ast::RecordField *,
                     const ast::BinarySegment *, const ast::BinaryModifier *, const ast::BranchClause *,
                     const ast::IfClause *, const ast::ReceiveTimeout *, const ast::CatchClause *,
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
    // Charge scheduled objects before growing the explicit traversal stack.
    std::size_t visits_;

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
    void prefix(const Work &work) const;
    // Dispatch arena handles through the module's ownership-checked accessors.
    void visit(const ast::FormId &id);
    void visit(const ast::ExprId &id);
    void visit(const ast::TermId &id);
    void visit(const ast::TypeId &id);
    // Render arities as compact scalar entries while retaining declaration order.
    void arities(const std::vector<ast::NameArity> &values) const;
    void visit(const ast::PatternSyntaxId &id);

    template <typename T> void visit(const T *value) { (*this)(*value); }

    // Decode printable integer characters from either arena.
    std::optional<char32_t> string_character(const ast::ExprId &id) const;
    std::optional<char32_t> string_character(const ast::TermId &id) const;

    // Compact nonempty integer lists while retaining the traversal's element budget.
    template <typename Id> bool string_list(const std::vector<Id> &elements) {
        if (elements.empty() || elements.size() > visits_) {
            return false;
        }
        std::u32string text;
        text.reserve(elements.size());
        for (const auto &id : elements) {
            const auto character = string_character(id);
            if (!character) {
                return false;
            }
            text += *character;
        }
        visits_ -= elements.size();
        (*this)(ast::StringLiteral{std::move(text)});
        return true;
    }

    // Preserve positional roles for ordered arena handles and embedded objects.
    template <typename Range> void handles(const std::string_view role, const Range &values) {
        std::size_t index = 0;
        for (const auto &id : values) {
            child(std::string(role) + '[' + std::to_string(index++) + ']', id);
        }
    }

    template <typename T> void objects(const std::string_view role, const std::vector<T> &values) {
        std::size_t index = 0;
        for (const auto &value : values) {
            child(std::string(role) + '[' + std::to_string(index++) + ']', &value);
        }
    }
};
} // namespace erlang_aot::printing
