// Added for parse transforms: block expressions, funs and comprehensions as abstract format terms.
#include "export_writer.hpp"

namespace clause::transforms {
namespace {
// The end index of a body item: an expression or a maybe conditional match.
std::size_t item_end(const ast::Module &syntax, const std::variant<ast::ExprId, ast::MaybeMatch> &item) {
    if (const auto *expression = std::get_if<ast::ExprId>(&item)) {
        return syntax.expression(*expression).source.end;
    }
    return std::get<ast::MaybeMatch>(item).source.end;
}

struct Control {
    AbstractWriter &writer_;
    const ast::Expression &node_;

    TermId own() const { return writer_.first(node_.source); }

    TermId make(const std::u32string_view tag, const TermId anno, std::vector<TermId> rest) const {
        return writer_.node(tag, anno, std::move(rest));
    }

    Terms &terms() const { return writer_.terms(); }

    const ast::Module &syntax() const { return writer_.syntax(); }

    // The annotation of the token right after a pattern.
    TermId after(const ast::PatternSyntaxId &pattern) const {
        return writer_.token(node_.source, syntax().pattern(pattern).source.end);
    }

    TermId operator()(const ast::BlockExpression &value) const {
        return make(U"block", own(), {writer_.expressions(value.body)});
    }

    TermId operator()(const ast::CaseExpression &value) const {
        return make(U"case", own(), {writer_.expression(value.value), writer_.branch_clauses(value.clauses)});
    }

    // If clauses have no patterns and are annotated at their first guard test.
    TermId operator()(const ast::IfExpression &value) const {
        std::vector<TermId> clauses;
        clauses.reserve(value.clauses.size());
        for (const auto &clause : value.clauses) {
            const auto &first = clause.guard.alternatives.front().tests.front();
            const auto anno = writer_.first(syntax().expression(first).source);
            clauses.push_back(writer_.clause(anno, {}, clause.guard, clause.body));
        }
        return make(U"if", own(), {terms().list(std::move(clauses))});
    }

    TermId operator()(const ast::ReceiveExpression &value) const {
        const auto clauses = writer_.branch_clauses(value.clauses);
        if (!value.after) {
            return make(U"receive", own(), {clauses});
        }
        return make(U"receive", own(),
                    {clauses, writer_.expression(value.after->timeout), writer_.expressions(value.after->body)});
    }

    TermId operator()(const ast::FunExpression &value) const {
        std::vector<TermId> clauses;
        clauses.reserve(value.clauses.size());
        for (const auto &clause : value.clauses) {
            clauses.push_back(writer_.function_clause(clause));
        }
        auto list = terms().list(std::move(clauses));
        if (value.name) {
            return make(U"named_fun", own(), {terms().atom(value.name->name), list});
        }
        return make(U"fun", own(), {terms().tuple({terms().atom(U"clauses"), list})});
    }

    TermId operator()(const ast::TryExpression &value) const {
        const auto of = value.of ? writer_.branch_clauses(*value.of) : terms().nil();
        std::vector<TermId> handlers;
        if (value.handlers) {
            for (const auto &handler : *value.handlers) {
                handlers.push_back(catch_clause(handler));
            }
        }
        const auto after = value.after ? writer_.expressions(*value.after) : terms().nil();
        return make(U"try", own(), {writer_.expressions(value.body), of, terms().list(std::move(handlers)), after});
    }

    // {clause, A, [{tuple, A, [Class, Reason, Stack]}], Guard, Body}: an omitted class is `throw` at the reason, an
    // omitted stacktrace `_` at the reason's last annotation.
    TermId catch_clause(const ast::CatchClause &handler) const {
        const auto reason = writer_.pattern(handler.reason);
        TermId anno = 0;
        TermId exception_class = 0;
        if (handler.exception_class) {
            anno = writer_.first(syntax().expression(*handler.exception_class).source);
            exception_class = writer_.expression(*handler.exception_class);
        } else {
            anno = writer_.first(syntax().pattern(handler.reason).source);
            exception_class = writer_.atom_node(anno, U"throw");
        }
        const auto stack = handler.stacktrace ? writer_.expression(*handler.stacktrace)
                                              : writer_.var_node(writer_.last_anno(reason), U"_");
        const auto tuple = make(U"tuple", anno, {terms().list({exception_class, reason, stack})});
        return writer_.clause(anno, {tuple}, handler.guard, handler.body);
    }

    TermId operator()(const ast::MaybeExpression &value) const {
        std::vector<TermId> body;
        body.reserve(value.body.size());
        for (const auto &item : value.body) {
            if (const auto *expression = std::get_if<ast::ExprId>(&item)) {
                body.push_back(writer_.expression(*expression));
                continue;
            }
            const auto &match = std::get<ast::MaybeMatch>(item);
            body.push_back(make(U"maybe_match", after(match.pattern),
                                {writer_.pattern(match.pattern), writer_.expression(match.value)}));
        }
        auto list = terms().list(std::move(body));
        if (!value.otherwise) {
            return make(U"maybe", own(), {list});
        }
        const auto else_anno = writer_.token(node_.source, item_end(syntax(), value.body.back()));
        const auto otherwise = make(U"else", else_anno, {writer_.branch_clauses(*value.otherwise)});
        return make(U"maybe", own(), {list, otherwise});
    }

    template <typename Value> TermId operator()(const Value &) const { return export_comprehension(writer_, node_); }
};

struct Comprehension {
    AbstractWriter &writer_;
    const ast::Expression &node_;

    TermId own() const { return writer_.first(node_.source); }

    TermId make(const std::u32string_view tag, const TermId anno, std::vector<TermId> rest) const {
        return writer_.node(tag, anno, std::move(rest));
    }

    Terms &terms() const { return writer_.terms(); }

    const ast::Module &syntax() const { return writer_.syntax(); }

    TermId after(const ast::PatternSyntaxId &pattern) const {
        return writer_.token(node_.source, syntax().pattern(pattern).source.end);
    }

    // One template, or a list of them when there are several.
    TermId templates(std::vector<TermId> items) const {
        return items.size() == 1 ? items.front() : terms().list(std::move(items));
    }

    TermId map_field(const ast::MapField &field) const {
        const auto tag = field.kind == ast::MapFieldKind::exact ? U"map_field_exact" : U"map_field_assoc";
        const auto anno = writer_.token(node_.source, syntax().expression(field.key).source.end);
        return make(tag, anno, {writer_.expression(field.key), writer_.expression(field.value)});
    }

    TermId operator()(const ast::ListComprehension &value) const {
        std::vector<TermId> items;
        items.reserve(value.templates.size());
        for (const auto &item : value.templates) {
            items.push_back(writer_.expression(item));
        }
        return make(U"lc", own(), {templates(std::move(items)), qualifiers(value.qualifiers)});
    }

    TermId operator()(const ast::MapComprehension &value) const {
        std::vector<TermId> items;
        items.reserve(value.templates.size());
        for (const auto &item : value.templates) {
            items.push_back(map_field(item));
        }
        return make(U"mc", own(), {templates(std::move(items)), qualifiers(value.qualifiers)});
    }

    TermId operator()(const ast::BinaryComprehension &value) const {
        return make(U"bc", own(), {writer_.expression(value.expression), qualifiers(value.qualifiers)});
    }

    template <typename Value> TermId operator()(const Value &) const {
        throw TermError("expression has no abstract format");
    }

    // A zip group is annotated at its first qualifier when it is last, else at the comma after it (erl_parse).
    TermId qualifiers(const std::vector<ast::ComprehensionQualifier> &items) const {
        std::vector<TermId> result;
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (const auto *simple = std::get_if<ast::Qualifier>(&items[index])) {
                result.push_back(qualifier(*simple));
                continue;
            }
            const auto &zip = std::get<ast::ZippedQualifier>(items[index]);
            std::vector<TermId> members;
            members.reserve(zip.qualifiers.size());
            for (const auto &member : zip.qualifiers) {
                members.push_back(qualifier(member));
            }
            const auto anno = index + 1 == items.size() ? terms().node(members.front()).children_[1]
                                                        : writer_.token(zip.source, zip.source.end);
            result.push_back(make(U"zip", anno, {terms().list(std::move(members))}));
        }
        return terms().list(std::move(result));
    }

    TermId qualifier(const ast::Qualifier &item) const {
        if (const auto *filter = std::get_if<ast::FilterQualifier>(&item.value)) {
            return writer_.expression(filter->expression);
        }
        if (const auto *list = std::get_if<ast::ListGenerator>(&item.value)) {
            return make(list->strict ? U"generate_strict" : U"generate", after(list->pattern),
                        {writer_.pattern(list->pattern), writer_.expression(list->input)});
        }
        if (const auto *binary = std::get_if<ast::BinaryGenerator>(&item.value)) {
            return make(binary->strict ? U"b_generate_strict" : U"b_generate", after(binary->pattern),
                        {writer_.pattern(binary->pattern), writer_.expression(binary->input)});
        }
        const auto &map = std::get<ast::MapGenerator>(item.value);
        const auto field =
            make(U"map_field_exact", after(map.key), {writer_.pattern(map.key), writer_.pattern(map.value)});
        return make(map.strict ? U"m_generate_strict" : U"m_generate", after(map.value),
                    {field, writer_.expression(map.input)});
    }
};
} // namespace

TermId export_control(AbstractWriter &writer, const ast::Expression &expression) {
    return std::visit(Control{writer, expression}, expression.value);
}

TermId export_comprehension(AbstractWriter &writer, const ast::Expression &expression) {
    return std::visit(Comprehension{writer, expression}, expression.value);
}
} // namespace clause::transforms
