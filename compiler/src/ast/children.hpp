#pragma once
#include "builder.hpp"

namespace erlang_aot::ast {
// Enumerate every child-bearing payload; scalar overloads explicitly have no children.
struct Children {
    const Builder &builder;
    const OriginId &form;
    void operator()(const Specification &value) const;
    void signature(const SpecificationSignature &value) const;

    void child(const TypeId &id) const { source(builder.view().type(id).source); }

    void operator()(const TypeGroup &value) const;
    void operator()(const AnnotatedType &value) const;
    void operator()(const UnionType &value) const;
    void operator()(const RangeType &value) const;
    void operator()(const UnaryType &value) const;
    void operator()(const BinaryTypeOperator &value) const;
    void operator()(const TypeApplication &value) const;
    void operator()(const TupleType &value) const;
    void operator()(const ListType &value) const;
    void operator()(const MapType &value) const;
    void operator()(const RecordType &value) const;
    void operator()(const BitstringType &value) const;
    void operator()(const FunType &value) const;
    void operator()(const TypeDeclaration &value) const;

    void child(const TermId &id) const { source(builder.view().term(id).source); }

    void operator()(const ModuleAttribute &) const {}

    void operator()(const FileAttribute &) const {}

    void operator()(const Function &value) const { function_clauses(value.clauses); }

    void operator()(const ExportAttribute &) const {}

    void operator()(const ImportAttribute &) const {}

    void operator()(const ImportRecordAttribute &) const {}

    void operator()(const GenericAttribute &value) const { child(value.value); }

    void operator()(const RecordDeclaration &value) const;
    void operator()(const DocumentationAttribute &value) const;
    void operator()(const TermTuple &value) const;
    void operator()(const TermList &value) const;
    void operator()(const TermMap &value) const;

    void operator()(const TermBits &) const {}

    void operator()(const TermFunction &) const {}

    // Validate complete control-flow payloads and their embedded source ranges.
    void body(const std::vector<ExprId> &values) const;
    void guard(const GuardSyntax &value) const;
    void pattern(const PatternSyntaxId &id, bool restricted) const;
    void branch(const BranchClause &value) const;
    void branches(const std::vector<BranchClause> &values) const;
    void operator()(const BlockExpression &value) const;
    void operator()(const CaseExpression &value) const;
    void operator()(const IfExpression &value) const;
    void operator()(const ReceiveExpression &value) const;
    // Share complete clause checks across functions, funs, catches and maybe bodies.
    void function_clause(const FunctionClause &value) const;
    void function_clauses(const std::vector<FunctionClause> &values) const;
    void handler(const CatchClause &value) const;
    void maybe_item(const ExprId &value) const;
    void maybe_item(const MaybeMatch &value) const;

    void operator()(const LocalFunReference &) const {}

    void operator()(const RemoteFunReference &) const {}

    void operator()(const FunExpression &value) const;
    void operator()(const TryExpression &value) const;
    void operator()(const MaybeExpression &value) const;
    // Preserve qualifier structure while enforcing owned candidates and nonempty groups.
    void qualifier(const Qualifier &value) const;
    void qualifier(const ZippedQualifier &value) const;
    void qualifiers(const std::vector<ComprehensionQualifier> &values) const;
    void qualifier_value(const FilterQualifier &value) const;
    void qualifier_value(const ListGenerator &value) const;
    void qualifier_value(const BinaryGenerator &value) const;
    void qualifier_value(const MapGenerator &value) const;
    void operator()(const ListComprehension &value) const;
    void operator()(const MapComprehension &value) const;
    void operator()(const BinaryComprehension &value) const;

    void child(const ExprId &id) const {
        if (builder.view().expression(id).source.form != form) {
            throw std::invalid_argument("AST child belongs to another form");
        }
    }

    // Validate nested syntax extents as well as expression child owners.
    void source(const NodeSource &value) const {
        if (value.form != form) {
            throw std::invalid_argument("AST field belongs to another form");
        }
        (void)builder.view().extent(value);
    }

    void operator()(const MapExpression &value) const {
        if (value.base) {
            child(*value.base);
        }
        for (const auto &field : value.fields) {
            source(field.source);
            child(field.key);
            child(field.value);
        }
    }

    void operator()(const RecordExpression &value) const {
        source(value.identity.source);
        if (value.base) {
            child(*value.base);
        }
        for (const auto &field : value.fields) {
            source(field.source);
            child(field.value);
        }
    }

    void operator()(const RecordAccess &value) const {
        source(value.identity.source);
        source(value.field_source);
        child(value.base);
    }

    void operator()(const RecordIndex &value) const {
        source(value.name_source);
        source(value.field_source);
    }

    void operator()(const Atom &) const {}

    void operator()(const Variable &) const {}

    void operator()(const IntegerLiteral &) const {}

    void operator()(const FloatLiteral &) const {}

    void operator()(const CharacterLiteral &) const {}

    void operator()(const StringLiteral &) const {}

    // Explicit type lists must be nonempty; semantic compatibility remains a later check.
    void segment(const BinarySegment &value) const {
        source(value.source);
        child(value.value);
        if (value.size) {
            child(*value.size);
        }
        if (value.modifiers) {
            if (value.modifiers->empty()) {
                throw std::invalid_argument("empty binary modifier list");
            }
            for (const auto &modifier : *value.modifiers) {
                source(modifier.source);
            }
        }
    }

    void operator()(const Bitstring &value) const {
        for (const auto &item : value.segments) {
            segment(item);
        }
    }

    void operator()(const UnaryExpression &value) const { child(value.operand); }

    void operator()(const BinaryExpression &value) const {
        child(value.left);
        child(value.right);
    }

    void operator()(const MatchExpression &value) const {
        child(value.left);
        child(value.right);
    }

    void operator()(const CatchExpression &value) const { child(value.expression); }

    void operator()(const RemoteExpression &value) const {
        child(value.module);
        child(value.function);
    }

    void operator()(const CallExpression &value) const {
        child(value.target);
        for (const auto &id : value.arguments) {
            child(id);
        }
    }

    void operator()(const Group &value) const { child(value.expression); }

    void operator()(const Tuple &value) const {
        for (const auto &id : value.elements) {
            child(id);
        }
    }

    void operator()(const List &value) const {
        if (value.tail && value.elements.empty()) {
            throw std::invalid_argument("list tail requires a head");
        }
        for (const auto &id : value.elements) {
            child(id);
        }
        if (value.tail) {
            child(*value.tail);
        }
    }
};
} // namespace erlang_aot::ast
