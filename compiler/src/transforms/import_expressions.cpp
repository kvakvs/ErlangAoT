// Added for parse transforms: abstract expressions and patterns as tokens (the inverse of export_expressions).
#include "annotations.hpp"
#include "import_writer.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace clause::transforms {
namespace {
// Precedence and associativity of erl_parse's binary operators.
struct Operator {
    std::u32string_view name_;
    int precedence_;
    // -1 left, 0 none, 1 right associative.
    int associativity_;
};

constexpr std::array<Operator, 27> OPERATORS{
    {{U"!", 100, 1},     {U"orelse", 150, 1}, {U"andalso", 160, 1}, {U"==", 200, 0},   {U"/=", 200, 0},
     {U"=<", 200, 0},    {U"<", 200, 0},      {U">=", 200, 0},      {U">", 200, 0},    {U"=:=", 200, 0},
     {U"=/=", 200, 0},   {U"++", 300, 1},     {U"--", 300, 1},      {U"+", 400, -1},   {U"-", 400, -1},
     {U"bor", 400, -1},  {U"bxor", 400, -1},  {U"bsl", 400, -1},    {U"bsr", 400, -1}, {U"or", 400, -1},
     {U"xor", 400, -1},  {U"*", 500, -1},     {U"/", 500, -1},      {U"div", 500, -1}, {U"rem", 500, -1},
     {U"band", 500, -1}, {U"and", 500, -1}}};

constexpr int PRIMARY = 1000;
constexpr int UNARY = 600;
constexpr int POSTFIX = 700;
constexpr int CALL = 750;

const Operator *binary_operator(const std::u32string_view name) {
    const auto found = std::ranges::find(OPERATORS, name, &Operator::name_);
    return found == OPERATORS.end() ? nullptr : &*found;
}

// Operators spelled as words are reserved words; the others are symbols.
void operator_token(FormImporter &importer, const std::u32string_view name, const Place &at) {
    if (name.front() >= U'a' && name.front() <= U'z') {
        importer.writer().keyword(name, at);
    } else {
        importer.writer().symbol(name, at);
    }
}

using Node = const std::vector<TermId> &;

void atom_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    importer.writer().atom(importer.atom(parts[2]), importer.place(id));
}

void var_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    importer.writer().variable(importer.atom(parts[2]), importer.place(id));
}

// Integers and floats; a negative value becomes a minus sign before its magnitude.
void number_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    const auto &value = importer.terms().node(parts[2]);
    if (value.kind_ == TermKind::integer) {
        auto digits = value.integer_;
        if (digits.decimal.starts_with('-')) {
            importer.writer().symbol(U"-", at);
            digits.decimal.erase(0, 1);
        }
        importer.writer().integer(digits, at);
    } else if (value.kind_ == TermKind::floating) {
        if (std::signbit(value.float_)) {
            importer.writer().symbol(U"-", at);
        }
        importer.writer().floating(std::abs(value.float_), at);
    } else {
        importer.fail(id, "expected a number");
    }
}

void char_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto code = importer.terms().small_integer(parts[2]);
    if (!code || *code < 0 || *code > 0x10FFFF) {
        importer.fail(id, "expected a character code");
    }
    importer.writer().character(*code, importer.place(id));
}

void string_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto text = importer.terms().text(parts[2]);
    if (!text) {
        importer.fail(id, "expected a string");
    }
    importer.writer().string(*text, importer.place(id));
}

void nil_node(FormImporter &importer, const TermId id) {
    importer.children<2>(id);
    const auto at = importer.place(id);
    importer.writer().symbol(U"[", at);
    importer.writer().symbol(U"]", at);
}

// Whether a cons cell continues the list written before it (`, B`): erl_parse annotates such cells at their head;
// a cell annotated elsewhere was written as its own `[B ...]` after a `|`.
bool continues(const FormImporter &importer, const TermId cell) {
    const auto &terms = importer.terms();
    const auto &parts = terms.node(cell).children_;
    const auto head = first_annotation(terms, parts.at(2));
    return !head || same_place(terms, parts.at(1), *head) || !annotation(terms, parts.at(1));
}

// [A, B | T] from a cons chain, iteratively.
void cons_node(FormImporter &importer, const TermId id) {
    importer.writer().symbol(U"[", importer.place(id));
    auto cell = id;
    for (bool first = true; importer.tag(cell) == U"cons" && (first || continues(importer, cell)); first = false) {
        const auto &parts = importer.children<4>(cell);
        if (!first) {
            importer.writer().symbol(U",", importer.place(cell));
        }
        importer.expression(parts[2]);
        cell = parts[3];
    }
    if (importer.tag(cell) == U"nil") {
        importer.writer().symbol(U"]", importer.place(cell));
        return;
    }
    importer.writer().symbol(U"|", importer.place(cell));
    importer.expression(cell);
    importer.writer().symbol(U"]", importer.place(cell));
}

void tuple_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    importer.writer().symbol(U"{", at);
    importer.sequence(parts[2]);
    importer.writer().symbol(U"}", at);
}

// Type specifiers after `/`: names, or Name:Value, separated by `-`.
void bin_types(FormImporter &importer, const TermId types, const Place &at) {
    if (importer.terms().is_atom(types, U"default")) {
        return;
    }
    importer.writer().symbol(U"/", at);
    const auto &list = importer.items(types);
    for (std::size_t index = 0; index < list.size(); ++index) {
        if (index != 0) {
            importer.writer().symbol(U"-", at);
        }
        const auto &type = importer.terms().node(list[index]);
        if (type.kind_ == TermKind::atom) {
            importer.writer().atom(type.atom_, at);
            continue;
        }
        if (type.kind_ != TermKind::tuple || type.children_.size() != 2) {
            importer.error("invalid binary type specifier");
        }
        importer.writer().atom(importer.atom(type.children_[0]), at);
        importer.writer().symbol(U":", at);
        importer.writer().integer(importer.terms().node(type.children_[1]).integer_, at);
    }
}

void bin_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto at = importer.place(id);
    importer.writer().symbol(U"<<", at);
    const auto &elements = importer.items(parts[2]);
    for (std::size_t index = 0; index < elements.size(); ++index) {
        const auto element = elements[index];
        if (importer.tag(element) != U"bin_element") {
            importer.fail(element, "expected a bin_element");
        }
        const auto &segment = importer.children<5>(element);
        const auto place = importer.place(element);
        if (index != 0) {
            importer.writer().symbol(U",", place);
        }
        importer.expression(segment[2], PRIMARY);
        if (!importer.terms().is_atom(segment[3], U"default")) {
            importer.writer().symbol(U":", place);
            importer.expression(segment[3], PRIMARY);
        }
        bin_types(importer, segment[4], place);
    }
    importer.writer().symbol(U">>", at);
}

void op_node(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<4, 5>(id);
    const auto name = importer.atom(parts[2]);
    const auto at = importer.place(id);
    if (size == 4) {
        operator_token(importer, name, at);
        importer.expression(parts[3], UNARY);
        return;
    }
    const auto *info = binary_operator(name);
    if (!info) {
        importer.fail(id, "unknown operator " + utf8(name));
    }
    importer.expression(parts[3], info->precedence_ + (info->associativity_ < 0 ? 0 : 1));
    operator_token(importer, name, at);
    importer.expression(parts[4], info->precedence_ + (info->associativity_ > 0 ? 0 : 1));
}

void match_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    importer.expression(parts[2], 101);
    importer.writer().symbol(U"=", at);
    importer.expression(parts[3], 100);
}

void catch_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    importer.writer().keyword(U"catch", importer.place(id));
    importer.expression(parts[2]);
}

void remote_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    importer.expression(parts[2], PRIMARY);
    importer.writer().symbol(U":", at);
    importer.expression(parts[3], PRIMARY);
}

void call_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    if (importer.tag(parts[2]) == U"remote") {
        remote_node(importer, parts[2]);
    } else {
        importer.expression(parts[2], CALL);
    }
    importer.writer().symbol(U"(", at);
    importer.sequence(parts[3]);
    importer.writer().symbol(U")", at);
}

// The base of a record or map update: a primary expression or another update/access, else parenthesized.
void update_base(FormImporter &importer, const TermId base) {
    const auto level = precedence(importer, base);
    importer.expression(base, level == POSTFIX ? POSTFIX : PRIMARY);
}

void map_fields(FormImporter &importer, const TermId list) {
    const auto &fields = importer.items(list);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto field = fields[index];
        const auto kind = importer.tag(field);
        if (kind != U"map_field_assoc" && kind != U"map_field_exact") {
            importer.fail(field, "expected a map field");
        }
        const auto &parts = importer.children<4>(field);
        const auto at = importer.place(field);
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.expression(parts[2]);
        importer.writer().symbol(kind == U"map_field_exact" ? U":=" : U"=>", at);
        importer.expression(parts[3]);
    }
}

void map_node(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<4, 3>(id);
    const auto at = importer.place(id);
    if (size == 4) {
        update_base(importer, parts[2]);
    }
    importer.writer().symbol(U"#", at);
    importer.writer().symbol(U"{", at);
    map_fields(importer, parts.back());
    importer.writer().symbol(U"}", at);
}

// `#name`, `#m:name` or `#_` of a record expression.
void record_name(FormImporter &importer, const TermId name, const Place &at) {
    const auto &node = importer.terms().node(name);
    if (node.kind_ == TermKind::list && node.children_.empty()) {
        importer.writer().symbol(U"#_", at);
        return;
    }
    importer.writer().symbol(U"#", at);
    if (node.kind_ == TermKind::tuple && node.children_.size() == 2) {
        importer.writer().atom(importer.atom(node.children_[0]), at);
        importer.writer().symbol(U":", at);
        importer.writer().atom(importer.atom(node.children_[1]), at);
        return;
    }
    importer.writer().atom(importer.atom(name), at);
}

void record_fields(FormImporter &importer, const TermId list) {
    const auto &fields = importer.items(list);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto field = fields[index];
        if (importer.tag(field) != U"record_field") {
            importer.fail(field, "expected a record field");
        }
        const auto &parts = importer.children<4>(field);
        const auto at = importer.place(field);
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.expression(parts[2]);
        importer.writer().symbol(U"=", at);
        importer.expression(parts[3]);
    }
}

void record_node(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<5, 4>(id);
    const auto at = importer.place(id);
    if (size == 5) {
        update_base(importer, parts[2]);
    }
    record_name(importer, parts[size - 2], at);
    importer.writer().symbol(U"{", at);
    record_fields(importer, parts.back());
    importer.writer().symbol(U"}", at);
}

void record_field_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<5>(id);
    const auto at = importer.place(id);
    update_base(importer, parts[2]);
    record_name(importer, parts[3], at);
    importer.writer().symbol(U".", at);
    importer.expression(parts[4]);
}

void record_index_node(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const auto at = importer.place(id);
    record_name(importer, parts[2], at);
    importer.writer().symbol(U".", at);
    importer.expression(parts[3]);
}

// A negative number is written with a unary minus.
int number_precedence(const Terms &terms, const TermId value) {
    const auto &node = terms.node(value);
    const bool negative =
        node.kind_ == TermKind::integer ? node.integer_.decimal.starts_with('-') : std::signbit(node.float_);
    return negative ? UNARY : PRIMARY;
}

// Whether a node is a record or map update, which has a base expression.
bool update(const std::u32string_view kind, const std::size_t size) {
    return (kind == U"record" && size == 5) || (kind == U"map" && size == 4);
}

using Handler = void (*)(FormImporter &, TermId);

struct Entry {
    std::u32string_view tag_;
    Handler handler_;
};

constexpr std::array<Entry, 18> HANDLERS{{{U"atom", atom_node},
                                          {U"var", var_node},
                                          {U"integer", number_node},
                                          {U"float", number_node},
                                          {U"char", char_node},
                                          {U"string", string_node},
                                          {U"nil", nil_node},
                                          {U"cons", cons_node},
                                          {U"tuple", tuple_node},
                                          {U"bin", bin_node},
                                          {U"op", op_node},
                                          {U"match", match_node},
                                          {U"catch", catch_node},
                                          {U"call", call_node},
                                          {U"remote", remote_node},
                                          {U"map", map_node},
                                          {U"record", record_node},
                                          {U"record_field", record_field_node}}};
} // namespace

void import_expression(FormImporter &importer, const TermId id) {
    const auto kind = importer.tag(id);
    const auto found = std::ranges::find(HANDLERS, kind, &Entry::tag_);
    if (found != HANDLERS.end()) {
        found->handler_(importer, id);
    } else if (kind == U"record_index") {
        record_index_node(importer, id);
    } else {
        import_control(importer, id);
    }
}

int precedence(const FormImporter &importer, const TermId id) {
    static constexpr std::array<std::pair<std::u32string_view, int>, 5> FIXED{
        {{U"catch", 0}, {U"match", 100}, {U"record_field", POSTFIX}, {U"call", CALL}, {U"remote", 800}}};
    const auto kind = importer.tag(id);
    const auto found = std::ranges::find(FIXED, kind, &std::pair<std::u32string_view, int>::first);
    if (found != FIXED.end()) {
        return found->second;
    }
    const auto &parts = importer.terms().node(id).children_;
    if (kind == U"op") {
        const auto *info = parts.size() == 5 ? binary_operator(importer.terms().node(parts[2]).atom_) : nullptr;
        return info ? info->precedence_ : UNARY;
    }
    if (update(kind, parts.size())) {
        return POSTFIX;
    }
    return kind == U"integer" || kind == U"float" ? number_precedence(importer.terms(), parts.back()) : PRIMARY;
}
} // namespace clause::transforms
