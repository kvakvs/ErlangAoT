// Added for parse transforms: block expressions, funs and comprehensions as tokens (inverse of export_control).
#include "annotations.hpp"
#include "import_writer.hpp"
#include <algorithm>
#include <array>

namespace clause::transforms {
namespace {
// The primary-expression precedence: binary comprehension templates and remote fun parts need no operators.
constexpr int PRIMARY = 1000;

void block_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"begin", at);
    importer.sequence(parts[2]);
    importer.writer().keyword(U"end", at);
}

// `if Guard -> Body; ... end`: if clauses have no patterns.
void if_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"if", at);
    const auto &clauses = importer.items(parts[2]);
    for (std::size_t index = 0; index < clauses.size(); ++index) {
        const auto &clause = importer.children<5>(clauses[index]);
        const auto place = importer.place(clauses[index]);
        if (index != 0) {
            importer.writer().symbol(U";", place);
        }
        importer.guard(clause[3]);
        importer.writer().symbol(U"->", place);
        importer.sequence(clause[4]);
    }
    importer.writer().keyword(U"end", at);
}

void case_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"case", at);
    importer.expression(parts[2]);
    importer.writer().keyword(U"of", at);
    importer.clauses(parts[3], false);
    importer.writer().keyword(U"end", at);
}

void receive_node(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<5, 3>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"receive", at);
    importer.clauses(parts[2], false);
    if (size == 5) {
        importer.writer().keyword(U"after", at);
        importer.expression(parts[3]);
        importer.writer().symbol(U"->", at);
        importer.sequence(parts[4]);
    }
    importer.writer().keyword(U"end", at);
}

// Whether a node is annotated at the given annotation.
bool annotated_at(const Terms &terms, const TermId node, const std::optional<TermId> anno) {
    return anno && same_place(terms, terms.node(node).children_.at(1), *anno);
}

// erl_parse writes an omitted stacktrace as `_` at the reason's last annotation.
bool omitted_stack(const FormImporter &importer, const TermId stack, const TermId reason) {
    const auto &terms = importer.terms();
    return importer.tag(stack) == U"var" && terms.is_atom(terms.node(stack).children_[2], U"_") &&
           annotated_at(terms, stack, last_annotation(terms, reason));
}

// ... and an omitted class as `throw` at the reason's first annotation, with an omitted stacktrace.
bool omitted_class(const FormImporter &importer, const std::vector<TermId> &triple) {
    const auto &terms = importer.terms();
    return importer.tag(triple[0]) == U"atom" && terms.is_atom(terms.node(triple[0]).children_[2], U"throw") &&
           annotated_at(terms, triple[0], first_annotation(terms, triple[1])) &&
           omitted_stack(importer, triple[2], triple[1]);
}

// The {Class, Reason, Stack} pattern of a catch clause.
const std::vector<TermId> &catch_pattern(FormImporter &importer, const TermId clause) {
    const auto &patterns = importer.items(importer.children<5>(clause)[2]);
    if (patterns.size() != 1 || importer.tag(patterns[0]) != U"tuple") {
        importer.fail(clause, "expected {Class, Reason, Stacktrace} in a catch clause");
    }
    const auto &triple = importer.items(importer.children<3>(patterns[0])[2]);
    if (triple.size() != 3) {
        importer.fail(clause, "expected {Class, Reason, Stacktrace} in a catch clause");
    }
    return triple;
}

// A catch clause `[Class:]Reason[:Stack]`: an implicit throw or stacktrace stays implicit.
void catch_clause(FormImporter &importer, const TermId clause) {
    const auto &triple = catch_pattern(importer, clause);
    const auto at = importer.place(clause);
    if (!omitted_class(importer, triple)) {
        importer.expression(triple[0]);
        importer.writer().symbol(U":", at);
    }
    importer.expression(triple[1]);
    if (!omitted_stack(importer, triple[2], triple[1])) {
        importer.writer().symbol(U":", at);
        importer.expression(triple[2]);
    }
    importer.clause_rest(clause);
}

void try_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<6>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"try", at);
    importer.sequence(parts[2]);
    if (!importer.items(parts[3]).empty()) {
        importer.writer().keyword(U"of", at);
        importer.clauses(parts[3], false);
    }
    const auto &handlers = importer.items(parts[4]);
    if (!handlers.empty()) {
        importer.writer().keyword(U"catch", at);
        for (std::size_t index = 0; index < handlers.size(); ++index) {
            if (index != 0) {
                importer.writer().symbol(U";", importer.place(handlers[index]));
            }
            catch_clause(importer, handlers[index]);
        }
    }
    if (!importer.items(parts[5]).empty()) {
        importer.writer().keyword(U"after", at);
        importer.sequence(parts[5]);
    }
    importer.writer().keyword(U"end", at);
}

// A part of `fun M:F/A`: an abstract atom/variable/integer node, or the plain value older forms use.
void fun_part(FormImporter &importer, const TermId part, const Place &at) {
    const auto &node = importer.terms().node(part);
    if (node.kind_ == TermKind::atom) {
        importer.writer().atom(node.atom_, at);
    } else if (node.kind_ == TermKind::integer) {
        importer.writer().integer(node.integer_, at);
    } else {
        importer.expression(part, PRIMARY);
    }
}

void fun_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"fun", at);
    const auto &body = importer.terms().node(parts[2]);
    const auto kind = importer.tag(parts[2]);
    if (kind == U"clauses" && body.children_.size() == 2) {
        importer.clauses(body.children_[1], true);
        importer.writer().keyword(U"end", at);
        return;
    }
    if (kind != U"function" || (body.children_.size() != 3 && body.children_.size() != 4)) {
        importer.fail(id, "malformed fun");
    }
    if (body.children_.size() == 4) {
        fun_part(importer, body.children_[1], at);
        importer.writer().symbol(U":", at);
    }
    fun_part(importer, body.children_[body.children_.size() - 2], at);
    importer.writer().symbol(U"/", at);
    fun_part(importer, body.children_.back(), at);
}

void named_fun_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    const auto name = importer.atom(parts[2]);
    importer.writer().keyword(U"fun", at);
    const auto &clauses = importer.items(parts[3]);
    for (std::size_t index = 0; index < clauses.size(); ++index) {
        const auto place = importer.place(clauses[index]);
        if (index != 0) {
            importer.writer().symbol(U";", place);
        }
        importer.writer().variable(name, place);
        importer.writer().symbol(U"(", place);
        importer.sequence(importer.children<5>(clauses[index])[2]);
        importer.writer().symbol(U")", place);
        importer.clause_rest(clauses[index]);
    }
    importer.writer().keyword(U"end", at);
}

void maybe_node(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<4, 3>(id);
    const auto at = importer.place(id);
    importer.writer().keyword(U"maybe", at);
    const auto &body = importer.items(parts[2]);
    for (std::size_t index = 0; index < body.size(); ++index) {
        if (index != 0) {
            importer.writer().symbol(U",", importer.place(body[index]));
        }
        if (importer.tag(body[index]) != U"maybe_match") {
            importer.expression(body[index]);
            continue;
        }
        const auto &match = importer.children<4>(body[index]);
        importer.expression(match[2], 101);
        importer.writer().symbol(U"?=", importer.place(body[index]));
        importer.expression(match[3]);
    }
    if (size == 4) {
        const auto &otherwise = importer.children<3>(parts[3]);
        importer.writer().keyword(U"else", importer.place(parts[3]));
        importer.clauses(otherwise[2], false);
    }
    importer.writer().keyword(U"end", at);
}

// Generators and filters of a comprehension, zip groups joined by `&&`.
void qualifier(FormImporter &importer, const TermId id);

void qualifiers(FormImporter &importer, const TermId list, const std::u32string_view separator) {
    const auto &items = importer.items(list);
    for (std::size_t index = 0; index < items.size(); ++index) {
        // A zip group that is not last is annotated at the comma after it (erl_parse).
        if (index != 0) {
            const auto previous = items[index - 1];
            const auto at = importer.tag(previous) == U"zip" ? previous : items[index];
            importer.writer().symbol(separator, importer.place(at));
        }
        qualifier(importer, items[index]);
    }
}

struct Generator {
    std::u32string_view tag_;
    std::u32string_view arrow_;
};

constexpr std::array<Generator, 6> GENERATORS{{{U"generate", U"<-"},
                                               {U"generate_strict", U"<:-"},
                                               {U"b_generate", U"<="},
                                               {U"b_generate_strict", U"<:="},
                                               {U"m_generate", U"<-"},
                                               {U"m_generate_strict", U"<:-"}}};

void qualifier(FormImporter &importer, const TermId id) {
    const auto kind = importer.tag(id);
    if (kind == U"zip") {
        qualifiers(importer, importer.children<3>(id)[2], U"&&");
        return;
    }
    const auto found = std::ranges::find(GENERATORS, kind, &Generator::tag_);
    if (found == GENERATORS.end()) {
        importer.expression(id);
        return;
    }
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    if (kind.starts_with(U"m_")) {
        const auto &field = importer.children<4>(parts[2]);
        importer.expression(field[2]);
        importer.writer().symbol(U":=", importer.place(parts[2]));
        importer.expression(field[3]);
    } else {
        importer.expression(parts[2]);
    }
    importer.writer().symbol(found->arrow_, at);
    importer.expression(parts[3]);
}

// The templates of a map comprehension: one map field, or a list of them.
void map_templates(FormImporter &importer, const TermId templates) {
    const bool many = importer.terms().node(templates).kind_ == TermKind::list;
    const std::vector<TermId> fields = many ? importer.items(templates) : std::vector<TermId>{templates};
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto kind = importer.tag(fields[index]);
        const auto &parts = importer.children<4>(fields[index]);
        const auto at = importer.place(fields[index]);
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.expression(parts[2]);
        importer.writer().symbol(kind == U"map_field_exact" ? U":=" : U"=>", at);
        importer.expression(parts[3]);
    }
}

// `[T || Q]`, `<<T || Q>>` and `#{K => V || Q}`; several templates come as a list.
void comprehension_node(FormImporter &importer, const TermId id) {
    const auto kind = importer.tag(id);
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    const bool many = importer.terms().node(parts[2]).kind_ == TermKind::list;
    if (kind == U"bc") {
        importer.writer().symbol(U"<<", at);
        importer.expression(parts[2], PRIMARY);
    } else if (kind == U"lc") {
        importer.writer().symbol(U"[", at);
        if (many) {
            importer.sequence(parts[2]);
        } else {
            importer.expression(parts[2]);
        }
    } else {
        importer.writer().symbol(U"#", at);
        importer.writer().symbol(U"{", at);
        map_templates(importer, parts[2]);
    }
    importer.writer().symbol(U"||", at);
    qualifiers(importer, parts[3], U",");
    importer.writer().symbol(kind == U"bc" ? U">>" : kind == U"lc" ? U"]" : U"}", at);
}

using Handler = void (*)(FormImporter &, TermId);

struct Entry {
    std::u32string_view tag_;
    Handler handler_;
};

constexpr std::array<Entry, 11> HANDLERS{{{U"block", block_node},
                                          {U"if", if_node},
                                          {U"case", case_node},
                                          {U"receive", receive_node},
                                          {U"try", try_node},
                                          {U"fun", fun_node},
                                          {U"named_fun", named_fun_node},
                                          {U"maybe", maybe_node},
                                          {U"lc", comprehension_node},
                                          {U"bc", comprehension_node},
                                          {U"mc", comprehension_node}}};
} // namespace

void import_control(FormImporter &importer, const TermId id) {
    const auto found = std::ranges::find(HANDLERS, importer.tag(id), &Entry::tag_);
    if (found == HANDLERS.end()) {
        importer.fail(id, "unknown expression " + utf8(importer.tag(id)));
    }
    found->handler_(importer, id);
}
} // namespace clause::transforms
