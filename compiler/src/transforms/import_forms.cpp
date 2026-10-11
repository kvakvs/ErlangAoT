// Added for parse transforms: function and attribute forms as tokens (inverse of export_forms).
#include "import_writer.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace clause::transforms {
namespace {
// `[f/1, g/2]` of export and import attributes.
void name_arities(FormImporter &importer, const TermId list, const Place &at) {
    const auto &entries = importer.items(list);
    importer.writer().symbol(U"[", at);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &pair = importer.terms().node(entries[index]);
        if (pair.kind_ != TermKind::tuple || pair.children_.size() != 2 ||
            importer.terms().node(pair.children_[1]).kind_ != TermKind::integer) {
            importer.error("expected {Name, Arity}");
        }
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.writer().atom(importer.atom(pair.children_[0]), at);
        importer.writer().symbol(U"/", at);
        importer.writer().integer(importer.terms().node(pair.children_[1]).integer_, at);
    }
    importer.writer().symbol(U"]", at);
}

// One record declaration field: `name`, `name = Default`, either with `:: Type`; a separator first unless first.
void record_field(FormImporter &importer, TermId field, const bool first) {
    std::optional<TermId> type;
    if (importer.tag(field) == U"typed_record_field") {
        const auto &parts = importer.children<3>(field);
        field = parts[1];
        type = parts[2];
    }
    if (importer.tag(field) != U"record_field") {
        importer.error("expected a record field");
    }
    const auto &parts = importer.children<3, 4>(field);
    const auto place = importer.place(field);
    if (!first) {
        importer.writer().symbol(U",", place);
    }
    importer.expression(parts[2]);
    if (parts.size() == 4) {
        importer.writer().symbol(U"=", place);
        importer.expression(parts[3]);
    }
    if (type) {
        importer.writer().symbol(U"::", place);
        importer.type(*type);
    }
}

void record_fields(FormImporter &importer, const TermId list, const Place &at) {
    const auto &fields = importer.items(list);
    importer.writer().symbol(U"{", at);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        record_field(importer, fields[index], index == 0);
    }
    importer.writer().symbol(U"}", at);
}

// One signature `(Args) -> Result [when Constraints]` of a spec or callback.
void signature(FormImporter &importer, const TermId id) {
    auto function = id;
    std::optional<TermId> constraints;
    const auto &terms = importer.terms();
    if (importer.tag(id) == U"type" && terms.node(id).children_.size() == 4 &&
        terms.is_atom(terms.node(id).children_[2], U"bounded_fun")) {
        const auto &pair = importer.items(terms.node(id).children_[3]);
        if (pair.size() != 2) {
            importer.fail(id, "malformed bounded_fun");
        }
        function = pair[0];
        constraints = pair[1];
    }
    const auto &parts = importer.children<4>(function);
    const auto &product = importer.items(parts[3]);
    if (!terms.is_atom(parts[2], U"fun") || product.size() != 2) {
        importer.fail(function, "expected a function type");
    }
    const auto at = importer.place(function);
    importer.writer().symbol(U"(", at);
    importer.types(importer.children<4>(product[0])[3]);
    importer.writer().symbol(U")", at);
    importer.writer().symbol(U"->", at);
    importer.type(product[1]);
    if (!constraints) {
        return;
    }
    importer.writer().keyword(U"when", at);
    const auto &items = importer.items(*constraints);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto &constraint = importer.children<4>(items[index]);
        const auto &pair = importer.items(importer.items(constraint[3])[1]);
        const auto place = importer.place(items[index]);
        if (index != 0) {
            importer.writer().symbol(U",", place);
        }
        importer.type(pair.at(0));
        importer.writer().symbol(U"::", place);
        importer.type(pair.at(1));
    }
}

// -spec/-callback: {Name, Arity} or {Module, Name, Arity} and the signatures.
void specification(FormImporter &importer, const TermId value, const Place &at) {
    const auto &parts = importer.terms().node(value).children_;
    if (importer.terms().node(value).kind_ != TermKind::tuple || parts.size() != 2) {
        importer.error("malformed specification");
    }
    const auto &name = importer.terms().node(parts[0]).children_;
    if (name.size() == 3) {
        importer.writer().atom(importer.atom(name[0]), at);
        importer.writer().symbol(U":", at);
    }
    importer.writer().atom(importer.atom(name.at(name.size() - 2)), at);
    const auto &signatures = importer.items(parts[1]);
    for (std::size_t index = 0; index < signatures.size(); ++index) {
        if (index != 0) {
            importer.writer().symbol(U";", at);
        }
        signature(importer, signatures[index]);
    }
}

// -type Name(Vars) :: Type.
void type_declaration(FormImporter &importer, const TermId value, const Place &at) {
    const auto &parts = importer.terms().node(value).children_;
    if (importer.terms().node(value).kind_ != TermKind::tuple || parts.size() != 3) {
        importer.error("malformed type declaration");
    }
    importer.writer().atom(importer.atom(parts[0]), at);
    importer.writer().symbol(U"(", at);
    importer.sequence(parts[2]);
    importer.writer().symbol(U")", at);
    importer.writer().symbol(U"::", at);
    importer.type(parts[1]);
}

// -doc/-moduledoc: a literal, or a metadata map whose equiv value is a call expression.
void documentation(FormImporter &importer, const TermId value, const Place &at) {
    const auto &node = importer.terms().node(value);
    if (node.kind_ != TermKind::map) {
        importer.literal(value, at);
        return;
    }
    importer.writer().symbol(U"#", at);
    importer.writer().symbol(U"{", at);
    for (std::size_t index = 0; index < node.children_.size(); index += 2) {
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.literal(node.children_[index], at);
        importer.writer().symbol(U"=>", at);
        const auto item = node.children_[index + 1];
        if (importer.terms().is_atom(node.children_[index], U"equiv") && importer.tag(item) == U"call") {
            importer.expression(item);
        } else {
            importer.literal(item, at);
        }
    }
    importer.writer().symbol(U"}", at);
}

// The module name, or a parameterized module's {Name, [Variable]}.
void module_value(FormImporter &importer, const TermId value, const Place &at) {
    const auto &node = importer.terms().node(value);
    if (node.kind_ != TermKind::tuple) {
        importer.writer().atom(importer.atom(value), at);
        return;
    }
    importer.writer().atom(importer.atom(node.children_.at(0)), at);
    importer.writer().symbol(U",", at);
    importer.writer().symbol(U"[", at);
    const auto &variables = importer.items(node.children_.at(1));
    for (std::size_t index = 0; index < variables.size(); ++index) {
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.writer().variable(importer.atom(variables[index]), at);
    }
    importer.writer().symbol(U"]", at);
}

// `Module, [f/1]` of -import, `Module, [name]` of -import_record.
void import_value(FormImporter &importer, const bool functions, const std::vector<TermId> &parts, const Place &at) {
    importer.writer().atom(importer.atom(parts.at(0)), at);
    importer.writer().symbol(U",", at);
    if (functions) {
        name_arities(importer, parts.at(1), at);
    } else {
        importer.literal(parts.at(1), at);
    }
}

// The value inside `-name(...)` of the attributes with their own syntax; false for a plain literal.
bool special_value(FormImporter &importer, const std::u32string_view name, const TermId value, const Place &at) {
    const auto &parts = importer.terms().node(value).children_;
    if (name == U"module") {
        module_value(importer, value, at);
    } else if (name == U"export") {
        name_arities(importer, value, at);
    } else if (name == U"import" || name == U"import_record") {
        import_value(importer, name == U"import", parts, at);
    } else if (name == U"record") {
        importer.writer().atom(importer.atom(parts.at(0)), at);
        importer.writer().symbol(U",", at);
        record_fields(importer, parts.at(1), at);
    } else if (name == U"doc" || name == U"moduledoc") {
        documentation(importer, value, at);
    } else {
        return false;
    }
    return true;
}

// The file attribute: the following forms belong to its file; an explicit -file carries generated=true.
bool file_attribute(FormImporter &importer, const TermId form, const TermId value) {
    const auto &terms = importer.terms();
    const auto &parts = terms.node(value).children_;
    const auto name = parts.size() == 2 ? terms.text(parts[0]) : std::nullopt;
    if (!name || terms.node(parts[1]).kind_ != TermKind::integer) {
        importer.fail(form, "malformed file attribute");
    }
    importer.locator().set_file(utf8(*name));
    const auto at = importer.place(form);
    importer.writer().symbol(U"-", at);
    importer.writer().atom(U"file", at);
    importer.writer().symbol(U"(", at);
    importer.writer().string(*name, at);
    importer.writer().symbol(U",", at);
    importer.writer().integer(terms.node(parts[1]).integer_, at);
    importer.writer().symbol(U")", at);
    const auto &anno = terms.node(terms.node(form).children_[1]);
    return anno.kind_ == TermKind::list && std::ranges::any_of(anno.children_, [&](const TermId property) {
               const auto &pair = terms.node(property);
               return pair.kind_ == TermKind::tuple && pair.children_.size() == 2 &&
                      terms.is_atom(pair.children_[0], U"generated") && terms.is_atom(pair.children_[1], U"true");
           });
}

// Attributes written without parentheses: -spec, -callback, -type, -opaque, -nominal, native -record; false for
// the others.
bool unparenthesized(FormImporter &importer, const std::u32string_view name, const TermId value, const Place &at) {
    auto &writer = importer.writer();
    if (name == U"spec" || name == U"callback") {
        writer.atom(name, at);
        specification(importer, value, at);
    } else if (name == U"type" || name == U"opaque" || name == U"nominal") {
        writer.atom(name, at);
        type_declaration(importer, value, at);
    } else if (name == U"native_record") {
        const auto &record = importer.terms().node(value).children_;
        writer.atom(U"record", at);
        writer.symbol(U"#", at);
        writer.atom(importer.atom(record.at(0)), at);
        record_fields(importer, record.at(1), at);
    } else {
        return false;
    }
    return true;
}
} // namespace

void import_function(FormImporter &importer, const TermId form) {
    const auto &parts = importer.children<5>(form);
    const auto name = importer.atom(parts[2]);
    const auto &clauses = importer.items(parts[4]);
    if (clauses.empty()) {
        importer.fail(form, "function without clauses");
    }
    for (std::size_t index = 0; index < clauses.size(); ++index) {
        const auto at = importer.place(clauses[index]);
        if (index != 0) {
            importer.writer().symbol(U";", at);
        }
        importer.writer().atom(name, at);
        importer.writer().symbol(U"(", at);
        importer.sequence(importer.children<5>(clauses[index])[2]);
        importer.writer().symbol(U")", at);
        importer.clause_rest(clauses[index]);
    }
}

std::optional<bool> import_attribute(FormImporter &importer, const TermId form) {
    const auto &parts = importer.children<4>(form);
    const auto name = importer.atom(parts[2]);
    if (name == U"file") {
        return file_attribute(importer, form, parts[3]);
    }
    const auto at = importer.place(form);
    auto &writer = importer.writer();
    writer.symbol(U"-", at);
    if (unparenthesized(importer, name, parts[3], at)) {
        return std::nullopt;
    }
    writer.atom(name, at);
    writer.symbol(U"(", at);
    if (!special_value(importer, name, parts[3], at)) {
        importer.literal(parts[3], at);
    }
    writer.symbol(U")", at);
    return std::nullopt;
}

void FormImporter::literal(const TermId id, const Place &at) {
    if (++depth_ > 512) {
        throw ImportError(at, "attribute value nesting too deep");
    }
    const auto &node = terms_.node(id);
    switch (node.kind_) {
    case TermKind::atom:
        writer_.atom(node.atom_, at);
        break;
    case TermKind::integer:
    case TermKind::floating:
        literal_number(node, at);
        break;
    case TermKind::bits:
        literal_bits(node, at);
        break;
    case TermKind::list:
        literal_list(id, node, at);
        break;
    default:
        literal_container(node, at);
    }
    --depth_;
}

void FormImporter::literal_number(const TermNode &node, const Place &at) {
    if (node.kind_ == TermKind::integer) {
        auto digits = node.integer_;
        if (digits.decimal.starts_with('-')) {
            writer_.symbol(U"-", at);
            digits.decimal.erase(0, 1);
        }
        writer_.integer(digits, at);
        return;
    }
    if (std::signbit(node.float_)) {
        writer_.symbol(U"-", at);
    }
    writer_.floating(std::abs(node.float_), at);
}

void FormImporter::literal_bits(const TermNode &node, const Place &at) {
    writer_.symbol(U"<<", at);
    for (std::size_t index = 0; index < node.bytes_.size(); ++index) {
        if (index != 0) {
            writer_.symbol(U",", at);
        }
        const auto byte = static_cast<std::uint8_t>(node.bytes_[index]);
        const auto tail = index + 1 == node.bytes_.size() ? node.bit_count_ % 8 : 0;
        writer_.integer(Integer{std::to_string(tail == 0 ? byte : byte >> (8 - tail))}, at);
        if (tail != 0) {
            writer_.symbol(U":", at);
            writer_.integer(Integer{std::to_string(tail)}, at);
        }
    }
    writer_.symbol(U">>", at);
}

void FormImporter::literal_list(const TermId id, const TermNode &node, const Place &at) {
    if (const auto text = node.children_.empty() ? std::nullopt : terms_.text(id)) {
        writer_.string(*text, at);
        return;
    }
    writer_.symbol(U"[", at);
    const auto count = node.children_.size() - (node.improper_ ? 1 : 0);
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            writer_.symbol(U",", at);
        }
        literal(node.children_[index], at);
    }
    if (node.improper_) {
        writer_.symbol(U"|", at);
        literal(node.children_.back(), at);
    }
    writer_.symbol(U"]", at);
}

void FormImporter::literal_container(const TermNode &node, const Place &at) {
    const bool map = node.kind_ == TermKind::map;
    if (map) {
        writer_.symbol(U"#", at);
    }
    writer_.symbol(U"{", at);
    for (std::size_t index = 0; index < node.children_.size(); ++index) {
        if (index != 0) {
            writer_.symbol(map && index % 2 == 1 ? U"=>" : U",", at);
        }
        literal(node.children_[index], at);
    }
    writer_.symbol(U"}", at);
}
} // namespace clause::transforms
