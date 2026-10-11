#pragma once
// Added for parse transforms: shared state of the abstract-format-to-tokens importer.
#include "import.hpp"
#include <map>

namespace clause::transforms {
// Where a token comes from: its logical location, and the matching span of the real file when it is known.
struct Place {
    LogicalLocation location_;
    Span real_;
};

// A malformed abstract form, reported at the place of the offending node.
class ImportError : public std::runtime_error {
  public:
    ImportError(Place place, const std::string &message) : std::runtime_error(message), place_(std::move(place)) {}

    Place place_;
};

// Collects one form's tokens; their spelling is a synthetic text of the tokens, their locations the annotations.
class TokenWriter {
  public:
    void atom(std::u32string_view name, const Place &at) { add(TokenKind::atom, std::u32string(name), name, at); }

    void variable(std::u32string_view name, const Place &at) {
        add(TokenKind::variable, std::u32string(name), name, at);
    }

    void keyword(std::u32string_view word, const Place &at) { add(TokenKind::keyword, std::u32string(word), word, at); }

    void symbol(std::u32string_view text, const Place &at) { add(TokenKind::symbol, std::u32string(text), text, at); }

    void integer(const Integer &value, const Place &at);
    void floating(double value, const Place &at);
    void character(std::int64_t code, const Place &at);
    void string(std::u32string_view text, const Place &at);

    // The form's tokens and its dot. With `marker`, every token's related span is the text "file" so the form
    // reads back as an explicit -file directive; otherwise an empty related span marks a generated one.
    struct Form {
        std::vector<Token> tokens_;
        Token dot_;
    };

    Form finish(const Place &at, SourceManager &sources, const std::string &name,
                std::optional<bool> marker = std::nullopt);

  private:
    struct Pending {
        TokenKind kind_;
        TokenValue value_;
        std::size_t begin_;
        std::size_t end_;
        Place place_;
    };

    // The synthetic spelling text and the tokens written so far.
    std::u32string text_;
    std::vector<Pending> pending_;

    void add(TokenKind kind, TokenValue value, std::u32string_view spelling, const Place &at);
    // The related span marking a file attribute: the `file` token for an explicit -file, else an empty span.
    Span marker_span(const SourcePtr &source, bool explicit_file) const;
};

// Decodes annotations: {Line, Column}, Line, or a property list with location and file.
class Locator {
  public:
    explicit Locator(const ImportOptions &options) : options_(options), file_(options.main_file_) {}

    // The place of an annotation term in the current file; malformed ones throw ImportError at `fallback`.
    Place place(const Terms &terms, TermId anno, const Place &fallback);
    // The same without throwing: none for a malformed annotation.
    std::optional<Place> find(const Terms &terms, TermId anno);

    // Switch the file of later annotations (a file attribute).
    void set_file(std::string file) { file_ = std::move(file); }

    const std::string &file() const { return file_; }

  private:
    // Line start offsets of a known source.
    struct Lines {
        SourcePtr source_;
        std::vector<std::size_t> starts_;
    };

    const ImportOptions &options_;
    std::string file_;
    std::map<std::string, Lines> sources_;

    // The line starts of a file, read once.
    const Lines &lines(const std::string &file);
    // The span of the real file at a location; empty when the file is unknown.
    Span real(const std::string &file, std::size_t line, std::size_t column);
};

// Writes the tokens of one abstract form.
class FormImporter {
  public:
    FormImporter(const Terms &terms, Locator &locator, TokenWriter &writer)
        : terms_(terms), locator_(locator), writer_(writer) {}

    // Shapes: the tag of an abstract node, its children (checked count), list elements, atom names.
    std::u32string_view tag(TermId id) const;

    template <std::size_t SIZE, std::size_t OTHER = SIZE> const std::vector<TermId> &children(const TermId id) const {
        const auto &node = terms_.node(id);
        if (node.kind_ != TermKind::tuple || (node.children_.size() != SIZE && node.children_.size() != OTHER)) {
            malformed(id);
        }
        return node.children_;
    }

    const std::vector<TermId> &items(TermId list) const;
    std::u32string_view atom(TermId id) const;
    // The place of a node's annotation (its second element).
    Place place(TermId node);
    // Report a problem at a node's annotation when it has a valid one, else at the last place decoded.
    [[noreturn]] void fail(TermId node, const std::string &message);
    [[noreturn]] void error(const std::string &message) const;

    const Terms &terms() const { return terms_; }

    TokenWriter &writer() { return writer_; }

    Locator &locator() { return locator_; }

    // Expressions and patterns with at least the given operator precedence (lower ones get parentheses).
    void expression(TermId id, int minimum = 0);
    // Comma-separated expressions; a guard's alternatives separated by `;`.
    void sequence(TermId list, std::u32string_view separator = U",");
    void guard(TermId guard);
    // `Head [when Guard] -> Body` of a {clause, A, Patterns, Guard, Body}; the head is written by the caller.
    void clause_rest(TermId clause);
    // Clauses separated by `;`, each starting with its patterns in parentheses (function and fun clauses) or bare
    // (case, receive, try-of).
    void clauses(TermId list, bool parenthesized);
    void type(TermId id, bool top = true);
    void types(TermId list);
    // A plain term (attribute values) at a place.
    void literal(TermId id, const Place &at);

  private:
    // The forms' arena, the annotation decoder and the token sink of this form.
    const Terms &terms_;
    Locator &locator_;
    TokenWriter &writer_;
    // Nesting of expressions, types and literals, bounded like the parser's.
    std::size_t depth_ = 0;
    // The last annotation decoded, where errors in nodes without one are reported.
    Place last_;

    [[noreturn]] void malformed(TermId id) const;
    // Literal parts: numbers with a leading minus, bitstrings, lists (strings when printable), tuples and maps.
    void literal_number(const TermNode &node, const Place &at);
    void literal_bits(const TermNode &node, const Place &at);
    void literal_list(TermId id, const TermNode &node, const Place &at);
    void literal_container(const TermNode &node, const Place &at);
};

// Category importers (import_*.cpp).
void import_expression(FormImporter &importer, TermId id);
void import_control(FormImporter &importer, TermId id);
// Writes an attribute; for a file attribute also returns whether it is an explicit -file (true) or epp's (false).
std::optional<bool> import_attribute(FormImporter &importer, TermId form);
void import_function(FormImporter &importer, TermId form);
void import_type(FormImporter &importer, TermId id);
// Precedence of an expression for parenthesization: 0 catch ... 1000 primary.
int precedence(const FormImporter &importer, TermId id);
} // namespace clause::transforms
