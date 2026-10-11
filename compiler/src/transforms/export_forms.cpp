// Added for parse transforms: attributes as {attribute, Anno, Name, Value} (erl_parse build_attribute).
#include "export_writer.hpp"
#include <filesystem>

namespace clause::transforms {
namespace {
// Name/arity lists of export and import attributes: [{Name, Arity}].
TermId name_arities(Terms &terms, const std::vector<ast::NameArity> &entries) {
    std::vector<TermId> result;
    result.reserve(entries.size());
    for (const auto &entry : entries) {
        result.push_back(terms.tuple({terms.atom(entry.name.name), terms.integer(entry.arity)}));
    }
    return terms.list(std::move(result));
}

// The bits of a literal bitstring packed into bytes, high bits first.
TermId literal_bits(Terms &terms, const std::vector<bool> &bits) {
    std::string bytes((bits.size() + 7) / 8, '\0');
    for (std::size_t index = 0; index < bits.size(); ++index) {
        if (bits[index]) {
            bytes[index / 8] = static_cast<char>(static_cast<unsigned char>(bytes[index / 8]) | (0x80U >> (index % 8)));
        }
    }
    return terms.bits(std::move(bytes), bits.size());
}

// Plain Erlang values of literal attribute terms.
struct Literal {
    AbstractWriter &writer_;

    Terms &terms() const { return writer_.terms(); }

    TermId operator()(const ast::Atom &value) const { return terms().atom(value.name); }

    TermId operator()(const ast::IntegerLiteral &value) const { return terms().integer(value.value); }

    TermId operator()(const ast::FloatLiteral &value) const { return terms().floating(value.value); }

    TermId operator()(const ast::TermBits &value) const { return literal_bits(terms(), value.bits); }

    TermId operator()(const ast::TermTuple &value) const { return terms().tuple(children(value.elements)); }

    TermId operator()(const ast::TermList &value) const {
        auto elements = children(value.elements);
        if (!value.tail) {
            return terms().list(std::move(elements));
        }
        return terms().list(std::move(elements), writer_.literal(*value.tail));
    }

    TermId operator()(const ast::TermMap &value) const {
        std::vector<TermId> entries;
        for (const auto &[key, item] : value.entries) {
            entries.push_back(writer_.literal(key));
            entries.push_back(writer_.literal(item));
        }
        return terms().map(std::move(entries));
    }

    TermId operator()(const ast::TermFunction &) const {
        throw TermError("a fun in an attribute value has no abstract format term in Clause");
    }

    std::vector<TermId> children(const std::vector<ast::TermId> &items) const {
        std::vector<TermId> result;
        result.reserve(items.size());
        for (const auto &item : items) {
            result.push_back(writer_.literal(item));
        }
        return result;
    }
};

// The file name epp would give: Clause resolves includes to absolute paths, epp joins them to the including
// directory, which for files under the working directory is the path relative to it.
std::u32string epp_name(const std::u32string &name) {
    const auto bytes = utf8(name);
    const std::filesystem::path path(std::u8string(bytes.begin(), bytes.end()));
    std::error_code error;
    if (!path.is_absolute()) {
        return name;
    }
    const auto relative = std::filesystem::relative(path, std::filesystem::current_path(error), error);
    if (error || relative.empty() || *relative.begin() == "..") {
        return name;
    }
    const auto text = relative.generic_u8string();
    return Source(0, "file", std::string(text.begin(), text.end())).text;
}

// Whether the token spelling of a span is `text`.
bool spelled(const Span &span, const std::u32string_view text) {
    return span.source && std::u32string_view(span.source->text).substr(span.begin, span.end - span.begin) == text;
}

struct Attribute {
    AbstractWriter &writer_;
    const ast::Form &form_;

    Terms &terms() const { return writer_.terms(); }

    // The attribute name token after `-`, where erl_parse annotates every attribute.
    TermId anno() const { return writer_.token(form_.source, form_.source.begin + 1); }

    TermId make(const std::u32string_view name, const TermId value) const {
        return writer_.node(U"attribute", anno(), {terms().atom(name), value});
    }

    TermId operator()(const ast::ModuleAttribute &value) const {
        if (!value.parameters) {
            return make(U"module", terms().atom(value.name.name));
        }
        std::vector<TermId> parameters;
        parameters.reserve(value.parameters->size());
        for (const auto &parameter : *value.parameters) {
            parameters.push_back(terms().atom(parameter.name));
        }
        return make(U"module", terms().tuple({terms().atom(value.name.name), terms().list(std::move(parameters))}));
    }

    // An explicit -file keeps its own location, marked generated as epp does; the ones epp adds (at the start,
    // entering an include, resuming the includer) sit at {Line, 1} of the line they name.
    TermId operator()(const ast::FileAttribute &value) const {
        const auto &origin = writer_.syntax().anchor(form_.source);
        const bool explicit_file = !origin.related.empty() && spelled(origin.related.front(), U"file");
        auto location = terms().tuple({terms().integer(value.line), terms().integer(1)});
        if (explicit_file) {
            const auto generated = terms().tuple({terms().atom(U"generated"), terms().atom(U"true")});
            const auto at = terms().tuple({terms().atom(U"location"), writer_.anno(origin.location)});
            location = terms().list({generated, at});
        }
        const auto file = terms().tuple({terms().string(epp_name(value.name)), terms().integer(value.line)});
        return writer_.node(U"attribute", location, {terms().atom(U"file"), file});
    }

    TermId operator()(const ast::ExportAttribute &value) const {
        return make(U"export", name_arities(terms(), value.functions));
    }

    TermId operator()(const ast::ImportAttribute &value) const {
        return make(U"import",
                    terms().tuple({terms().atom(value.module.name), name_arities(terms(), value.functions)}));
    }

    TermId operator()(const ast::ImportRecordAttribute &value) const {
        std::vector<TermId> names;
        names.reserve(value.names.size());
        for (const auto &name : value.names) {
            names.push_back(terms().atom(name.name));
        }
        return make(U"import_record", terms().tuple({terms().atom(value.module.name), terms().list(std::move(names))}));
    }

    TermId operator()(const ast::GenericAttribute &value) const {
        return make(value.name.name, writer_.literal(value.value));
    }

    TermId operator()(const ast::RecordDeclaration &value) const {
        const auto record = terms().tuple({terms().atom(value.name.name), export_record_fields(writer_, value)});
        return make(value.native ? U"native_record" : U"record", record);
    }

    // -doc/-moduledoc: a literal value, or a metadata map whose equiv value is an expression.
    TermId operator()(const ast::DocumentationAttribute &value) const {
        const auto name = value.module ? U"moduledoc" : U"doc";
        if (const auto *term = std::get_if<ast::TermId>(&value.value)) {
            return make(name, writer_.literal(*term));
        }
        std::vector<TermId> entries;
        for (const auto &entry : std::get<std::vector<ast::DocumentationEntry>>(value.value)) {
            entries.push_back(writer_.literal(entry.key));
            const auto *literal = std::get_if<ast::TermId>(&entry.value);
            entries.push_back(literal ? writer_.literal(*literal)
                                      : writer_.expression(std::get<ast::ExprId>(entry.value)));
        }
        return make(name, terms().map(std::move(entries)));
    }

    TermId operator()(const ast::Specification &value) const { return export_specification(writer_, form_, value); }

    TermId operator()(const ast::TypeDeclaration &value) const {
        return export_type_declaration(writer_, form_, value);
    }

    TermId operator()(const ast::Function &) const { throw TermError("function forms are not attributes"); }
};
} // namespace

TermId AbstractWriter::literal(const ast::TermId &id) { return std::visit(Literal{*this}, syntax_.term(id).value); }

TermId export_attribute(AbstractWriter &writer, const ast::Form &form) {
    return std::visit(Attribute{writer, form}, form.value);
}
} // namespace clause::transforms
