#include "../terms/records.hpp"
#include "io_format.hpp"
#include "support.hpp"
#include "text.hpp"
#include <algorithm>
#include <erlang_aot/runtime/output.hpp>
#include <vector>

// ~p: io_lib_pretty:print/2 with unlimited depth and characters. A term becomes OTP's intermediate format (each
// value with its one-line length), then is laid out: what fits on the line is written whole, tagged tuples, maps
// and records choose an indentation, and other containers break between elements (docs/io.md).
namespace erlang_aot::runtime::builtins {
namespace {
// Containers nested deeper than this raise system_limit: the layout recurses once per level on the native stack,
// about 1.2 KiB per level in Debug builds (docs/differences.md).
constexpr std::size_t MAX_NESTING = 256;

// One value of the intermediate format.
struct Node {
    enum class Kind : std::uint8_t { text, binary, list, tuple, map, pair, record, field };
    Kind kind = Kind::text;
    // Leaf text; a record's "#module:name"; a field's name.
    std::u32string text;
    // Characters when written on one line.
    std::int64_t length = 0;
    // Elements of lists, tuples, maps (pairs) and records (fields); a pair's key and value; a field's value.
    std::vector<Node> children;
    // A binary's byte texts; the last is "Value:Bits" for a bitstring.
    std::vector<std::u32string> segments;
    // A tuple of at least two elements starting with an atom.
    bool tagged = false;
    // A list whose last child is the tail after '|'.
    bool improper = false;
    // A record's name length; a field's name length plus 3 (" = ").
    std::int64_t name_length = 0;
};

// A leaf node.
Node text_node(std::u32string text) {
    Node result;
    result.length = static_cast<std::int64_t>(text.size());
    result.text = std::move(text);
    return result;
}

// A container of `children` written as open + children joined by one-character separators + close.
Node container(Node::Kind kind, std::vector<Node> children, std::int64_t brackets) {
    Node result;
    result.kind = kind;
    result.length = brackets + std::max<std::int64_t>(static_cast<std::int64_t>(children.size()) - 1, 0);
    for (const auto &child : children) {
        result.length += child.length;
    }
    result.children = std::move(children);
    return result;
}

// A quoted string, escaped like io_lib:write_string/2.
std::u32string string_literal(std::u32string_view chars) {
    std::u32string out = U"\"";
    for (const auto c : chars) {
        static constexpr std::u32string_view NAMED = U"\n\r\t\v\b\f\x1B\x7F";
        static constexpr std::u32string_view LETTERS = U"nrtvbfed";
        if (c == '"' || c == '\\') {
            out += {U'\\', c};
        } else if (const auto at = NAMED.find(c); at != std::u32string_view::npos) {
            out += {U'\\', LETTERS[at]};
        } else if (c < 0x20 || (c >= 0x7F && c < 0xA0)) {
            out += {U'\\', U'0' + ((c >> 6U) & 7U), U'0' + ((c >> 3U) & 7U), U'0' + (c & 7U)};
        } else {
            out.push_back(c);
        }
    }
    return out + U"\"";
}

// Characters a string or binary may hold and still print as a string (io:printable_range() latin1).
bool printable(char32_t c) {
    static constexpr std::u32string_view CONTROLS = U"\n\r\t\v\b\f\x1B";
    return (c >= 0x20 && c <= 0x7E) || (c >= 0xA0 && c <= 0xFF) || CONTROLS.contains(c);
}

// The characters of a flat proper list of printable characters; none for any other list.
std::optional<std::u32string> printable_list(Term list) {
    std::u32string chars;
    for (; list.is_cons(); list = need(list.tail())) {
        const auto code = small(need(list.head()).word());
        if (!code || *code < 0 || *code > 0xFF || !printable(static_cast<char32_t>(*code))) {
            return std::nullopt;
        }
        chars.push_back(static_cast<char32_t>(*code));
    }
    return list.is_nil() ? std::optional{std::move(chars)} : std::nullopt;
}

// OTP's printable_unicode_bin with the Latin-1 printable range: the characters of printable UTF-8 bytes, empty
// when a character is not printable, none when the bytes are not UTF-8.
std::optional<std::u32string> printable_utf8(std::span<const std::byte> bytes) {
    std::u32string chars;
    chars.reserve(bytes.size());
    for (std::size_t at = 0; at < bytes.size();) {
        const auto c = next_utf8(bytes, at);
        if (!c) {
            return std::nullopt;
        }
        if (!printable(*c)) {
            return std::u32string{};
        }
        chars.push_back(*c);
    }
    return chars;
}

// Builds the intermediate format of a term (io_lib_pretty:print_length/7 without depth or character limits).
class Builder final {
  public:
    // Fix the atom style (t modifier) and whether lists and binaries may print as strings (no l modifier).
    Builder(bool unicode, bool strings) : unicode_(unicode), strings_(strings) {}

    // The node of `term` nested `depth` containers deep.
    Node node(const Term &term, std::size_t depth) {
        if (depth > MAX_NESTING) {
            throw FormatFailure{.reason = abi::v1::ErrorReason::system_limit};
        }
        if (term.is_cons()) {
            return list(term, depth);
        }
        if (term.is_tuple() && need(term.tuple_size()) > 0) {
            return tuple(term, depth);
        }
        if (term.is_map() && need(term.map_size()) > 0) {
            return map(term, depth);
        }
        if (term.is_native_record()) {
            return record(term, depth);
        }
        return term.is_bitstring() && need(term.bit_size()) > 0 ? binary(term) : text_node(write(term));
    }

    // io_lib:write/1 of a value, with atoms in the selected style.
    std::u32string write(const Term &term) const {
        return code_points(need(format_term(term, unicode_ ? TermStyle::write_unicode : TermStyle::write)));
    }

  private:
    // A printable flat list prints as a string; other lists element by element, an improper tail after '|'.
    Node list(Term term, std::size_t depth) {
        if (const auto chars = strings_ ? printable_list(term) : std::nullopt) {
            return text_node(string_literal(*chars));
        }
        std::vector<Node> children;
        for (; term.is_cons(); term = need(term.tail())) {
            children.push_back(node(need(term.head()), depth + 1));
        }
        const bool improper = !term.is_nil();
        if (improper) {
            children.push_back(node(term, depth + 1));
        }
        auto result = container(Node::Kind::list, std::move(children), 2);
        result.improper = improper;
        return result;
    }

    // Tuples are tagged when they start with an atom and have more elements.
    Node tuple(const Term &term, std::size_t depth) {
        const auto elements = need(term.tuple_elements());
        std::vector<Node> children;
        children.reserve(elements.size());
        for (const auto &element : elements) {
            children.push_back(node(element, depth + 1));
        }
        const bool tagged = children.size() > 1 && need(term.tuple_element(0)).is_atom();
        auto result = container(Node::Kind::tuple, std::move(children), 2);
        result.tagged = tagged;
        return result;
    }

    // Maps hold pairs "Key => Value" in key order.
    Node map(const Term &term, std::size_t depth) {
        const auto entries = need(term.map_entries());
        std::vector<Node> pairs;
        pairs.reserve(entries.size());
        for (const auto &[key, value] : entries) {
            std::vector<Node> children{node(key, depth + 1), node(value, depth + 1)};
            pairs.push_back(container(Node::Kind::pair, std::move(children), 3));
        }
        return container(Node::Kind::map, std::move(pairs), 3);
    }

    // Native records print as #module:name{field = Value, ...} in definition order.
    Node record(const Term &term, std::size_t depth) {
        const auto [module, name] = need(detail::record_identity(term));
        const auto values = need(term.record_fields());
        std::vector<Node> fields;
        fields.reserve(values.size());
        for (const auto &[field, value] : values) {
            auto child = node(value, depth + 1);
            const auto key = write(field);
            const auto name_length = static_cast<std::int64_t>(key.size()) + 3;
            auto entry = container(Node::Kind::field, {}, name_length + child.length);
            entry.text = key;
            entry.name_length = name_length;
            entry.children.push_back(std::move(child));
            fields.push_back(std::move(entry));
        }
        const auto text = U"#" + write(module) + U":" + write(name);
        auto result = container(Node::Kind::record, std::move(fields), static_cast<std::int64_t>(text.size()) + 2);
        result.text = text;
        result.name_length = static_cast<std::int64_t>(text.size());
        return result;
    }

    // Printable binaries print as strings; others as byte values.
    Node binary(const Term &term) {
        const auto bits = need(term.bit_size());
        const auto bytes = need(term.bitstring_bytes());
        if (strings_ && bits % 8 == 0) {
            if (const auto text = binary_string(bytes)) {
                return text_node(*text);
            }
        }
        std::vector<std::u32string> segments;
        segments.reserve(bytes.size());
        for (std::size_t i = 0; i < bits / 8; ++i) {
            segments.push_back(code_points(std::to_string(std::to_integer<unsigned>(bytes[i]))));
        }
        if (const auto rest = bits % 8; rest != 0) {
            const auto value = std::to_integer<unsigned>(bytes[bits / 8]) >> (8 - rest);
            segments.push_back(code_points(std::to_string(value) + ":" + std::to_string(rest)));
        }
        auto result = container(Node::Kind::binary, {}, 3 + static_cast<std::int64_t>(segments.size()));
        for (const auto &segment : segments) {
            result.length += static_cast<std::int64_t>(segment.size());
        }
        result.segments = std::move(segments);
        return result;
    }

    // <<"text">> for printable bytes; with t, <<"text"/utf8>> for printable UTF-8 beyond ASCII. With t, a
    // character that is not printable ends the scan; bytes that are not UTF-8 are read as Latin-1 instead.
    std::optional<std::u32string> binary_string(std::span<const std::byte> bytes) const {
        if (const auto text = unicode_ ? printable_utf8(bytes) : std::nullopt) {
            return text->empty() ? std::nullopt
                                 : std::optional{U"<<" + string_literal(*text) +
                                                 (text->size() == bytes.size() ? U">>" : U"/utf8>>")};
        }
        std::u32string chars;
        chars.reserve(bytes.size());
        std::ranges::transform(bytes, std::back_inserter(chars),
                               [](std::byte b) { return std::to_integer<char32_t>(b); });
        return std::ranges::all_of(chars, printable) ? std::optional{U"<<" + string_literal(chars) + U">>"}
                                                     : std::nullopt;
    }

    // The t modifier.
    bool unicode_;
    // Without the l modifier.
    bool strings_;
};

// An indentation that would pass half the line without room for the whole term (io_lib_pretty's no_good).
struct NoGood {};

// Where a record or field continues: the next column and indentation, whether a line break precedes it, and the
// width used on the current line.
struct RecordIndent {
    std::int64_t column;
    std::int64_t indent;
    bool newline;
    std::int64_t width;
};

// Lays out the intermediate format within a line length (io_lib_pretty pp/cind with depth and limits unused).
// Columns count from 1; `indent` is the number of spaces starting a continuation line; `last` (LD) is how many
// closing characters follow the node on its line; `width` (W) is what the line already holds of the container.
class Layout final {
  public:
    // Fix the line length and M, the one-line length of the whole term `root`.
    Layout(std::int64_t line_length, const Node &root) : line_(line_length), most_(root.length) {}

    // The tag indentation: -1, else 4, else 1, the first that keeps every chosen column within bounds.
    std::int64_t tag_indent(const Node &node, std::int64_t column) const {
        for (const std::int64_t indent : {-1, 4}) {
            try {
                return cind(node, column, indent, 0, 0);
            } catch (const NoGood &) {
                continue; // Try a wider indentation.
            }
        }
        return 1;
    }

    // Append `node` laid out from `column`.
    void pp(const Node &node, std::int64_t column, std::int64_t tag, std::int64_t indent, std::int64_t last,
            std::int64_t width) {
        if (fits(node, column, last, width)) {
            write(node);
            return;
        }
        switch (node.kind) {
        case Node::Kind::list:
        case Node::Kind::tuple:
            node.tagged ? tag_tuple(node, column, tag, indent, last, width + 1)
                        : bracketed(node, column + 1, tag, indent + 1, last, width + 1);
            break;
        case Node::Kind::map:
            out_ += U"#{";
            sequence(node, 0, Position{column + 2, column + 2, width + 1}, Frame{tag, indent + 2, last}, false);
            out_ += U"}";
            break;
        case Node::Kind::record:
            record(node, column, tag, indent, last, width + node.name_length + 1);
            break;
        case Node::Kind::binary:
            binary(node, column + 2, Frame{tag, indent + 2, last}, width);
            break;
        default:
            out_ += node.text;
        }
    }

    // Append `node` on one line.
    void write(const Node &node) {
        switch (node.kind) {
        case Node::Kind::text:
            out_ += node.text;
            return;
        case Node::Kind::binary:
            out_ += U"<<";
            for (std::size_t i = 0; i < node.segments.size(); ++i) {
                out_ += (i == 0 ? U"" : U",") + node.segments[i];
            }
            out_ += U">>";
            return;
        case Node::Kind::pair:
            write(node.children[0]);
            out_ += U" => ";
            write(node.children[1]);
            return;
        case Node::Kind::field:
            out_ += node.text + U" = ";
            write(node.children[0]);
            return;
        default:
            write_container(node);
        }
    }

    // The laid out text.
    std::u32string take() { return std::move(out_); }

  private:
    // Columns of a sequence: where continuation lines start, the current column and the line's width.
    struct Position {
        std::int64_t start;
        std::int64_t column;
        std::int64_t width;
    };

    // What every element of a sequence shares: the tag indentation, line indentation and closing characters.
    struct Frame {
        std::int64_t tag;
        std::int64_t indent;
        std::int64_t last;
    };

    // Whether `node` fits on the rest of the line.
    bool fits(const Node &node, std::int64_t column, std::int64_t last, std::int64_t width) const {
        return node.length < line_ - column - last && node.length + width + last <= most_;
    }

    // io_lib_pretty's ?ATM: text, a pair of texts or a field holding text, which never needs a line break.
    static bool atomic(const Node &node) {
        if (node.kind == Node::Kind::pair) {
            return node.children[0].kind == Node::Kind::text && node.children[1].kind == Node::Kind::text;
        }
        const auto &value = node.kind == Node::Kind::field ? node.children[0] : node;
        return value.kind == Node::Kind::text;
    }

    // Whether child `index` is an element (not past the end, not the improper tail).
    static bool element_at(const Node &node, std::size_t index) {
        return index < node.children.size() && !(node.improper && index + 1 == node.children.size());
    }

    // LD of child `index`: 0 when another element follows, else one more closing character.
    static std::int64_t last_of(const Node &node, std::size_t index, std::int64_t last) {
        return element_at(node, index + 1) ? 0 : last + 1;
    }

    // Whether an element of `length` (with its comma) stays on the current line after the previous one.
    bool stays(const Node &child, Position at, std::int64_t last) const {
        const auto length = child.length + 1;
        const bool room = last == 0 ? length + 1 < line_ - at.column && at.width + length + 1 <= most_
                                    : length < line_ - at.column - last && at.width + length + last <= most_;
        return room && atomic(child);
    }

    // A record's or field's continuation from `from`: on the same line `offset` further, or on a new line at the
    // tag indentation.
    static RecordIndent record_indent(RecordIndent from, std::int64_t offset, const Frame &frame) {
        const bool newline = frame.tag > 0 && offset > frame.tag;
        const auto step = newline ? frame.tag : offset;
        return {from.column + step, from.indent + step, newline, newline ? 0 : from.width};
    }

    // Append a newline and `indent` spaces.
    void newline(std::int64_t indent) {
        out_ += U"\n";
        out_.append(static_cast<std::size_t>(std::max<std::int64_t>(indent, 0)), U' ');
    }

    // Lists and untagged tuples: elements from `column` after the opening bracket.
    void bracketed(const Node &node, std::int64_t column, std::int64_t tag, std::int64_t indent, std::int64_t last,
                   std::int64_t width) {
        const bool list = node.kind == Node::Kind::list;
        out_ += list ? U"[" : U"{";
        sequence(node, 0, Position{column, column, width}, Frame{tag, indent, last}, true);
        out_ += list ? U"]" : U"}";
    }

    // A tagged tuple: the tag, then the elements aligned after "{Tag," or at the tag indentation.
    void tag_tuple(const Node &node, std::int64_t column, std::int64_t tag, std::int64_t indent, std::int64_t last,
                   std::int64_t width) {
        const auto &first = node.children[0];
        const auto offset = first.length + 2;
        out_ += U"{" + first.text;
        if (tag > 0 && offset > tag) {
            tail(node, 1, Position{column + tag, column + offset, width + first.length},
                 Frame{tag, indent + tag, last});
        } else {
            out_ += U",";
            sequence(node, 1, Position{column + offset, column + offset, width + first.length + 1},
                     Frame{tag, indent + offset, last}, true);
        }
        out_ += U"}";
    }

    // A record: its name, then its fields after "{" or on a new line at the tag indentation.
    void record(const Node &node, std::int64_t column, std::int64_t tag, std::int64_t indent, std::int64_t last,
                std::int64_t width) {
        out_ += node.text + U"{";
        if (!node.children.empty()) {
            const auto at =
                record_indent({column, indent, false, width}, node.name_length + 1, Frame{tag, indent, last});
            if (at.newline) {
                newline(at.indent);
            }
            sequence(node, 0, Position{at.column, at.column, at.width}, Frame{tag, at.indent, last}, true);
        }
        out_ += U"}";
    }

    // Children from `first`: the first placed at the start, the rest by `tail`. Maps restart the line width at the
    // first pair's width (`accumulate` false), as io_lib_pretty does.
    void sequence(const Node &node, std::size_t first, Position at, Frame frame, bool accumulate) {
        const auto used = element(node.children[first], at.start, frame, last_of(node, first, frame.last), at.width);
        tail(node, first + 1, Position{at.start, at.start + used, accumulate ? at.width + used : used}, frame);
    }

    // The remaining children: each stays on the line when it fits, else starts a new line at the indentation.
    void tail(const Node &node, std::size_t index, Position at, Frame frame) {
        for (; element_at(node, index); ++index) {
            const auto &child = node.children[index];
            const auto last = last_of(node, index, frame.last);
            if (stays(child, at, last)) {
                out_ += U",";
                write(child);
                at.column += child.length + 1;
                at.width += child.length + 1;
            } else {
                out_ += U",";
                newline(frame.indent);
                const auto used = element(child, at.start, frame, last, 0);
                at.column = at.start + used;
                at.width = used;
            }
        }
        if (index < node.children.size()) {
            improper_tail(node.children[index], at, frame);
        }
    }

    // Whether an improper list tail (with its '|') stays on the current line.
    bool tail_stays(const Node &tail, Position at, std::int64_t last) const {
        return tail.length + 1 < line_ - at.column - last && tail.length + 1 + at.width + last <= most_ && atomic(tail);
    }

    // An improper list tail after '|'.
    void improper_tail(const Node &tail, Position at, Frame frame) {
        out_ += U"|";
        const auto last = frame.last + 1;
        if (tail_stays(tail, at, last)) {
            write(tail);
            return;
        }
        newline(frame.indent);
        pp(tail, at.start, frame.tag, frame.indent, last, 0);
    }

    // One element at `column`; returns the width it leaves on its last line (the line length forces a break).
    std::int64_t element(const Node &node, std::int64_t column, Frame frame, std::int64_t last, std::int64_t width) {
        const bool fit = fits(node, column, last, width);
        if (fit && (node.kind == Node::Kind::pair || node.kind == Node::Kind::field || atomic(node))) {
            write(node);
            return atomic(node) ? node.length : line_;
        }
        if (node.kind == Node::Kind::pair) {
            pair(node, column, frame, last, width);
        } else if (node.kind == Node::Kind::field) {
            field(node, column, frame, last, width);
        } else {
            pp(node, column, frame.tag, frame.indent, last, width);
        }
        return line_;
    }

    // A map pair that does not fit: the key, then " =>" and the value on the next line, indented.
    void pair(const Node &node, std::int64_t column, Frame frame, std::int64_t last, std::int64_t width) {
        const auto step = frame.tag > 0 ? frame.tag : 4;
        pp(node.children[0], column, frame.tag, frame.indent, last, width);
        out_ += U" =>";
        newline(frame.indent + step);
        pp(node.children[1], column + step, frame.tag, frame.indent + step, last, 0);
    }

    // A record field that does not fit: "name = " and the value, or "name =" and the value on a new line.
    void field(const Node &node, std::int64_t column, Frame frame, std::int64_t last, std::int64_t width) {
        const auto at = record_indent({column, frame.indent, false, width + node.name_length}, node.name_length, frame);
        out_ += node.text + (at.newline ? U" =" : U" = ");
        if (at.newline) {
            newline(at.indent);
        }
        pp(node.children[0], at.column, frame.tag, at.indent, last, at.width);
    }

    // A binary's byte values, wrapped at max(8, room) characters per line.
    void binary(const Node &node, std::int64_t column, Frame frame, std::int64_t width) {
        const auto indent = frame.indent;
        const auto room = std::max<std::int64_t>(8, std::min(line_ - column, most_ - 4 - width) - frame.last);
        auto left = room;
        out_ += U"<<";
        for (std::size_t i = 0; i + 1 < node.segments.size(); ++i) {
            const auto length = static_cast<std::int64_t>(node.segments[i].size()) + 1;
            if (left - length < 0) {
                newline(indent);
                left = room;
            }
            out_ += node.segments[i] + U",";
            left -= length;
        }
        if (static_cast<std::int64_t>(node.segments.back().size()) > left) {
            newline(indent);
        }
        out_ += node.segments.back() + U">>";
    }

    // Lists, tuples, maps and records on one line.
    void write_container(const Node &node) {
        static constexpr std::array<std::u32string_view, 4> OPEN{U"[", U"{", U"#{", U"{"};
        static constexpr std::array<std::u32string_view, 4> CLOSE{U"]", U"}", U"}", U"}"};
        const auto kind = static_cast<std::size_t>(node.kind) - static_cast<std::size_t>(Node::Kind::list);
        const auto slot = node.kind == Node::Kind::record ? 3 : kind;
        out_ += node.kind == Node::Kind::record ? node.text : U"";
        out_ += OPEN.at(slot);
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            out_ += i == 0 ? U"" : (node.improper && i + 1 == node.children.size() ? U"|" : U",");
            write(node.children[i]);
        }
        out_ += CLOSE.at(slot);
    }

    // cind: the indentation `indent` if every chosen column stays within bounds, else NoGood.
    std::int64_t cind(const Node &node, std::int64_t column, std::int64_t indent, std::int64_t last,
                      std::int64_t width) const {
        if (fits(node, column, last, width)) {
            return indent;
        }
        switch (node.kind) {
        case Node::Kind::list:
        case Node::Kind::tuple:
            return node.tagged ? cind_tag_tuple(node, column, indent, last, width + 1)
                               : cind_sequence(node, 0, column + 1, Frame{indent, indent, last}, width + 1, false);
        case Node::Kind::map:
            return cind_sequence(node, 0, column + 2, Frame{indent, indent, last}, width + 2, false);
        case Node::Kind::record:
            return cind_record(node, column, indent, last, width + node.name_length + 1);
        default:
            return indent;
        }
    }

    // A column is acceptable when the whole term still fits after it, or it is within half the line.
    void bound(std::int64_t column, bool strict) const {
        const bool room = strict ? most_ + column < line_ : most_ + column <= line_;
        if (!room && !(strict ? column < line_ / 2 : column <= line_ / 2)) {
            throw NoGood{};
        }
    }

    // cind of a tagged tuple: its elements after the tag or at the tag indentation.
    std::int64_t cind_tag_tuple(const Node &node, std::int64_t column, std::int64_t indent, std::int64_t last,
                                std::int64_t width) const {
        const auto length = node.children[0].length;
        const auto offset = length + 2;
        const Frame frame{indent, indent, last};
        if (indent > 0 && offset > indent) {
            bound(column + indent, false);
            return cind_tail(node, 1, Position{column + indent, column + offset, width + length}, frame, false);
        }
        bound(column + offset, true);
        return cind_sequence(node, 1, column + offset, frame, width + length + 1, false);
    }

    // cind of a record's fields.
    std::int64_t cind_record(const Node &node, std::int64_t column, std::int64_t indent, std::int64_t last,
                             std::int64_t width) const {
        if (node.children.empty()) {
            return indent;
        }
        const auto at = cind_record_indent(node.name_length + 1, column, indent, width);
        return cind_sequence(node, 0, at.column, Frame{indent, indent, last}, at.width, true);
    }

    // record_indent for cind, with the column bound checked.
    RecordIndent cind_record_indent(std::int64_t offset, std::int64_t column, std::int64_t indent,
                                    std::int64_t width) const {
        const auto at = record_indent({column, 0, false, width}, offset, Frame{indent, indent, 0});
        bound(at.column, false);
        return at;
    }

    // cind of children from `first`.
    std::int64_t cind_sequence(const Node &node, std::size_t first, std::int64_t column, Frame frame,
                               std::int64_t width, bool fields) const {
        const auto used = cind_element(node.children[first], column, frame, last_of(node, first, frame.last), width);
        return cind_tail(node, first + 1, Position{column, column + used, width + used}, frame, fields);
    }

    // cind of the remaining children. Record fields continue from the current column, not the start (as OTP).
    std::int64_t cind_tail(const Node &node, std::size_t index, Position at, Frame frame, bool fields) const {
        for (; element_at(node, index); ++index) {
            const auto &child = node.children[index];
            const auto last = last_of(node, index, frame.last);
            if (stays(child, at, last)) {
                at.column += child.length + 1;
                at.width += child.length + 1;
            } else {
                const auto used = cind_element(child, at.start, frame, last, 0);
                at.column = (fields ? at.column : at.start) + used;
                at.width = used;
            }
        }
        if (index < node.children.size() && !tail_stays(node.children[index], at, frame.last + 1)) {
            cind(node.children[index], at.column, frame.indent, frame.last + 1, 0);
        }
        return frame.indent;
    }

    // cind of one element; returns the width it leaves, like `element`.
    std::int64_t cind_element(const Node &node, std::int64_t column, Frame frame, std::int64_t last,
                              std::int64_t width) const {
        const bool fit = fits(node, column, last, width);
        if (fit && (node.kind == Node::Kind::pair || node.kind == Node::Kind::field || atomic(node))) {
            return atomic(node) ? node.length : line_;
        }
        if (node.kind == Node::Kind::pair) {
            cind(node.children[0], column, frame.indent, last, width);
            const auto step = frame.indent > 0 ? frame.indent : 4;
            cind(node.children[1], column + step, frame.indent, last, 0);
        } else if (node.kind == Node::Kind::field) {
            const auto at = cind_record_indent(node.name_length, column, frame.indent, width + node.name_length);
            cind(node.children[0], at.column, frame.indent, last, at.width);
        } else {
            cind(node, column, frame.indent, last, width);
        }
        return line_;
    }

    // Line length (the ~p field width).
    std::int64_t line_;
    // M: the one-line length of the whole term.
    std::int64_t most_;
    // Text laid out so far.
    std::u32string out_;
};

// Whether io_lib_pretty lays a term out (rather than writing it with io_lib:write/1).
bool laid_out(const Term &term) {
    return term.is_tuple() || term.is_list() || term.is_map() || term.is_bitstring() || term.is_native_record();
}
} // namespace

std::u32string pretty(const Term &term, const PrettyOptions &options) {
    Builder builder(options.unicode, options.strings);
    if (!laid_out(term)) {
        return builder.write(term);
    }
    const auto column = std::max<std::int64_t>(options.column, 1);
    const auto node = builder.node(term, 0);
    Layout layout(options.line_length, node);
    if (options.line_length == 0 || node.length < options.line_length - column) {
        layout.write(node);
    } else {
        const auto tag = layout.tag_indent(node, column);
        layout.pp(node, column, tag, column - 1, 0, 0);
    }
    return layout.take();
}
} // namespace erlang_aot::runtime::builtins
