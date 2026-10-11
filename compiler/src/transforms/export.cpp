// Added for parse transforms: export entry point, annotations and clauses shared by every construct.
#include "export_writer.hpp"
#include "term_text.hpp"

namespace clause::transforms {
namespace {
// Whether a term is a {Line, Column} annotation.
bool annotation(const Terms &terms, const TermId id) {
    const auto &node = terms.node(id);
    return node.kind_ == TermKind::tuple && node.children_.size() == 2 && terms.small_integer(node.children_[0]) &&
           terms.small_integer(node.children_[1]);
}

// An annotation's location as a comparable pair.
std::pair<std::int64_t, std::int64_t> position(const Terms &terms, const TermId anno) {
    const auto &node = terms.node(anno);
    return {*terms.small_integer(node.children_[0]), *terms.small_integer(node.children_[1])};
}

// The later of an optional annotation and another one.
std::optional<TermId> later(const Terms &terms, const std::optional<TermId> latest, const TermId candidate) {
    return !latest || position(terms, *latest) < position(terms, candidate) ? candidate : *latest;
}

// The children of an abstract node that may hold annotations: all after the tag, except a bin_element's type list.
std::span<const TermId> annotated_children(const Terms &terms, const TermNode &node) {
    std::span<const TermId> children = node.children_;
    if (node.kind_ == TermKind::tuple && !children.empty() && terms.is_atom(children[0], U"bin_element")) {
        return children.first(std::min<std::size_t>(children.size(), 4));
    }
    return children;
}

// The main file's last line as numbered after an explicit -file directive of the main file: epp counts the
// directive's own line as the line it names.
std::optional<std::size_t> eof_line(const ast::Module &syntax, const ast::Form &form, const Source &main) {
    const auto *file = std::get_if<ast::FileAttribute>(&form.value);
    if (!file) {
        return std::nullopt;
    }
    const auto &related = syntax.anchor(form.source).related;
    if (related.empty() || related.front().source.get() != &main) {
        return std::nullopt;
    }
    const auto &site = related.front();
    if (std::u32string_view(main.text).substr(site.begin, site.end - site.begin) != U"file") {
        return std::nullopt;
    }
    const auto directive = main.position(related.front().begin).line;
    const auto last = main.position(main.text.size()).line;
    const auto named = small_integer(file->line);
    return named ? std::optional{last - directive + static_cast<std::size_t>(*named)} : std::nullopt;
}
} // namespace

TermId AbstractWriter::anno(const LogicalLocation &location) {
    return terms_.tuple({terms_.integer(static_cast<std::int64_t>(location.line)),
                         terms_.integer(static_cast<std::int64_t>(location.column))});
}

TermId AbstractWriter::first(const ast::NodeSource &source) {
    for (const auto &origin : syntax_.extent(source)) {
        const auto &span = origin.spelling;
        if (!span.source || std::u32string_view(span.source->text).substr(span.begin, span.end - span.begin) != U"(") {
            return anno(origin.location);
        }
    }
    return anchor(source);
}

TermId AbstractWriter::token(const ast::NodeSource &source, const std::size_t index) {
    return anno(syntax_.anchor({.form = source.form, .begin = index, .end = index + 1, .anchor = index}).location);
}

TermId AbstractWriter::anchor(const ast::NodeSource &source) { return anno(syntax_.anchor(source).location); }

std::u32string_view AbstractWriter::token_text(const ast::NodeSource &source, const std::size_t index) const {
    const auto &span =
        syntax_.anchor({.form = source.form, .begin = index, .end = index + 1, .anchor = index}).spelling;
    if (!span.source) {
        return {};
    }
    return std::u32string_view(span.source->text).substr(span.begin, span.end - span.begin);
}

TermId AbstractWriter::last_anno(const TermId pattern) {
    std::optional<TermId> latest;
    std::vector<TermId> pending{pattern};
    while (!pending.empty()) {
        const auto &node = terms_.node(pending.back());
        pending.pop_back();
        for (const auto child : annotated_children(terms_, node)) {
            if (annotation(terms_, child)) {
                latest = later(terms_, latest, child);
            } else {
                pending.push_back(child);
            }
        }
    }
    return latest ? *latest : anno({.file = {}, .line = 1, .column = 1});
}

TermId AbstractWriter::node(const std::u32string_view tag, const TermId anno, std::vector<TermId> rest) {
    rest.insert(rest.begin(), {terms_.atom(tag), anno});
    return terms_.tuple(std::move(rest));
}

TermId AbstractWriter::atom_node(const TermId anno, const std::u32string_view name) {
    return node(U"atom", anno, {terms_.atom(name)});
}

TermId AbstractWriter::var_node(const TermId anno, const std::u32string_view name) {
    return node(U"var", anno, {terms_.atom(name)});
}

TermId AbstractWriter::expressions(const std::vector<ast::ExprId> &items) {
    std::vector<TermId> result;
    result.reserve(items.size());
    for (const auto &item : items) {
        result.push_back(expression(item));
    }
    return terms_.list(std::move(result));
}

TermId AbstractWriter::pattern(const ast::PatternSyntaxId &id) {
    const auto &value = syntax_.pattern(id).value;
    return expression(std::visit([](const auto &item) { return item.expression; }, value));
}

TermId AbstractWriter::guard(const std::optional<ast::GuardSyntax> &guard) {
    std::vector<TermId> alternatives;
    if (guard) {
        for (const auto &alternative : guard->alternatives) {
            alternatives.push_back(expressions(alternative.tests));
        }
    }
    return terms_.list(std::move(alternatives));
}

TermId AbstractWriter::clause(const TermId anno, std::vector<TermId> patterns,
                              const std::optional<ast::GuardSyntax> &guard, const std::vector<ast::ExprId> &body) {
    return node(U"clause", anno, {terms_.list(std::move(patterns)), this->guard(guard), expressions(body)});
}

TermId AbstractWriter::function_clause(const ast::FunctionClause &clause) {
    std::vector<TermId> arguments;
    arguments.reserve(clause.arguments.size());
    for (const auto &argument : clause.arguments) {
        arguments.push_back(pattern(argument));
    }
    return this->clause(token(clause.source, clause.source.begin), std::move(arguments), clause.guard, clause.body);
}

TermId AbstractWriter::branch_clauses(const std::vector<ast::BranchClause> &clauses) {
    std::vector<TermId> result;
    result.reserve(clauses.size());
    for (const auto &item : clauses) {
        const auto anno = first(syntax_.pattern(item.pattern).source);
        result.push_back(clause(anno, {pattern(item.pattern)}, item.guard, item.body));
    }
    return terms_.list(std::move(result));
}

TermId AbstractWriter::types(const std::vector<ast::TypeId> &items) {
    std::vector<TermId> result;
    result.reserve(items.size());
    for (const auto &item : items) {
        result.push_back(type(item));
    }
    return terms_.list(std::move(result));
}

TermId AbstractWriter::form(const ast::Form &form) {
    const auto *function = std::get_if<ast::Function>(&form.value);
    if (!function) {
        return export_attribute(*this, form);
    }
    std::vector<TermId> clauses;
    clauses.reserve(function->clauses.size());
    for (const auto &item : function->clauses) {
        clauses.push_back(function_clause(item));
    }
    const auto anno = terms_.node(clauses.front()).children_[1];
    const auto arity = static_cast<std::int64_t>(function->clauses.front().arguments.size());
    return node(U"function", anno,
                {terms_.atom(function->name.name), terms_.integer(arity), terms_.list(std::move(clauses))});
}

AbstractModule export_module(const ast::Module &syntax, const std::span<const ast::FormId> forms, const Source &main) {
    AbstractModule result;
    AbstractWriter writer(syntax, result.terms_);
    auto end = main.position(main.text.size());
    for (const auto &id : forms) {
        const auto &form = syntax.form(id);
        result.forms_.push_back(writer.form(form));
        end.line = eof_line(syntax, form, main).value_or(end.line);
    }
    const auto eof = writer.anno({.file = {}, .line = end.line, .column = end.column});
    result.forms_.push_back(result.terms_.tuple({result.terms_.atom(U"eof"), eof}));
    return result;
}

std::string abstract_text(const AbstractModule &module) {
    std::string out;
    for (const auto form : module.forms_) {
        write_text(out, module.terms_, form);
        out += ".\n";
    }
    return out;
}
} // namespace clause::transforms
