#include "io_format.hpp"
#include "support.hpp"
#include "text.hpp"
#include <algorithm>
#include <cstdlib>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <variant>
#include <vector>

// io_lib:format(Format, Args) for the control sequences ~w ~p ~s ~c ~b ~B ~i ~n ~~ with field width, precision,
// pad character and the t, l and k modifiers. Format errors and unsupported sequences are badarg (docs/io.md).
// TODO(step 43A): formatting a large term should run in bounded portions.
namespace erlang_aot::runtime::builtins {
namespace {
// Largest field width or precision accepted; larger values would only exhaust memory.
constexpr std::int64_t MAX_FIELD = std::int64_t{1} << 24;

// How a chardata walk reads its characters and binaries.
enum class Charset : std::uint8_t {
    // unicode:characters_to_list: Unicode characters and UTF-8 binaries.
    unicode,
    // ~s: Latin-1 characters; binary bytes are characters.
    latin1,
    // ~ts: any non-negative characters; UTF-8 binaries, else their bytes.
    lenient,
};

// Append one character of a chardata leaf, or throw badarg for a character the charset rejects.
void append_character(const Term &leaf, Charset charset, std::u32string &out) {
    const auto code = small(leaf.word());
    const bool valid = code && (charset == Charset::unicode  ? unicode_character(*code)
                                : charset == Charset::latin1 ? *code >= 0 && *code <= 0xFF
                                                             : *code >= 0);
    if (!valid) {
        bad_argument();
    }
    // A lenient character beyond Unicode stays invalid; the output check rejects it unless a precision cuts it.
    out.push_back(static_cast<char32_t>(std::min(*code, MAX_CODE_POINT + 1)));
}

// Append the characters of a binary chardata element.
void append_binary(const Term &binary, Charset charset, std::u32string &out) {
    if (!binary.is_binary()) {
        bad_argument();
    }
    const auto bytes = need(binary.binary_bytes());
    if (charset != Charset::latin1) {
        if (auto text = decode_utf8(bytes)) {
            out += *text;
            return;
        }
        if (charset == Charset::unicode) {
            bad_argument();
        }
    }
    std::ranges::transform(bytes, std::back_inserter(out), [](std::byte b) { return std::to_integer<char32_t>(b); });
}

// Read one list until its end or a nested list, which is queued to be read before the rest of this one.
void read_chardata(Term list, Charset charset, std::u32string &out, std::vector<Term> &pending) {
    while (list.is_cons()) {
        auto element = need(list.head());
        list = need(list.tail());
        if (element.is_list()) {
            pending.push_back(std::move(list));
            pending.push_back(std::move(element));
            return;
        }
        element.is_binary() ? append_binary(element, charset, out) : append_character(element, charset, out);
    }
    if (!list.is_nil()) {
        append_binary(list, charset, out);
    }
}

// The characters of chardata: a binary, or a list of characters, binaries and nested chardata ending in [] or a
// binary.
std::u32string chardata(const Term &root, Charset charset) {
    std::u32string out;
    if (!root.is_list()) {
        append_binary(root, charset, out);
        return out;
    }
    std::vector<Term> pending{root};
    while (!pending.empty()) {
        auto list = std::move(pending.back());
        pending.pop_back();
        read_chardata(std::move(list), charset, out, pending);
    }
    return out;
}

// One element of a format: a character, or nested chardata of a list format, passed through unparsed.
struct Piece {
    char32_t code = 0;
    // The characters of a nested chardata element; it counts as one column (an io_lib_format quirk).
    std::optional<std::u32string> chunk = std::nullopt;
};

// Append the elements of a format list: characters, and nested chardata as chunks.
void list_pieces(Term list, std::vector<Piece> &pieces) {
    for (; !list.is_nil(); list = need(list.tail())) {
        if (!list.is_cons()) {
            bad_argument();
        }
        const auto element = need(list.head());
        const auto code = small(element.word());
        if (code && unicode_character(*code)) {
            pieces.push_back(Piece{static_cast<char32_t>(*code)});
        } else {
            pieces.push_back(Piece{0, chardata(element, Charset::unicode)});
        }
    }
}

// The pieces of a format: an atom's characters, a binary's bytes, or a list's elements.
std::vector<Piece> format_pieces(const Term &format) {
    std::vector<Piece> pieces;
    const auto add = [&](char32_t code) { pieces.push_back(Piece{code}); };
    if (format.is_atom()) {
        const auto chars = code_points(need(format.atom_spelling()));
        pieces.reserve(chars.size());
        std::ranges::for_each(chars, add);
    } else if (format.is_binary()) {
        const auto bytes = need(format.binary_bytes());
        pieces.reserve(bytes.size());
        std::ranges::for_each(bytes, [&](std::byte b) { add(std::to_integer<char32_t>(b)); });
    } else if (format.is_list()) {
        list_pieces(format, pieces);
    } else {
        bad_argument();
    }
    return pieces;
}

// One control sequence ~F.P.PadModC with its argument.
struct Control {
    // Field width F; `left` when it was negative ('-' or a negative '*' argument).
    std::optional<std::int64_t> width;
    bool left = false;
    // Precision P.
    std::optional<std::int64_t> precision;
    char32_t pad = ' ';
    // Modifiers t and l.
    bool unicode = false;
    bool strings = true;
    // Control character C and the argument it consumes, if any.
    char32_t letter = 0;
    std::optional<Term> argument;
};

// A format split into literal pieces and control sequences.
using Element = std::variant<Piece, Control>;

// Scans a format and its arguments into elements, as io_lib_format:scan/2 with all arguments consumed.
class Scanner final {
  public:
    // Borrow the pieces and start before the first argument.
    Scanner(const std::vector<Piece> &pieces, Term arguments) : pieces_(pieces), arguments_(std::move(arguments)) {}

    // Every element; badarg when a sequence is invalid or arguments are missing or left over.
    std::vector<Element> run() {
        std::vector<Element> elements;
        elements.reserve(pieces_.size());
        while (at_ < pieces_.size()) {
            const auto &piece = pieces_[at_++];
            if (!piece.chunk && piece.code == '~') {
                elements.emplace_back(control());
            } else {
                elements.emplace_back(piece);
            }
        }
        if (!arguments_.is_nil()) {
            bad_argument();
        }
        return elements;
    }

  private:
    // The next format character, unless the format ended or nested chardata follows.
    std::optional<char32_t> peek() const {
        return at_ < pieces_.size() && !pieces_[at_].chunk ? std::optional{pieces_[at_].code} : std::nullopt;
    }

    // The next format character, consumed only when it is `wanted`.
    bool accept(char32_t wanted) {
        const bool found = peek() == wanted;
        at_ += found ? 1 : 0;
        return found;
    }

    // The next format character, or badarg at the end of the format or before nested chardata.
    char32_t character() {
        if (at_ == pieces_.size() || pieces_[at_].chunk) {
            bad_argument();
        }
        return pieces_[at_++].code;
    }

    // The next argument, or badarg when none is left.
    Term argument() {
        if (!arguments_.is_cons()) {
            bad_argument();
        }
        auto head = need(arguments_.head());
        arguments_ = need(arguments_.tail());
        return head;
    }

    // A field value: '*' takes an integer argument, else decimal digits; none when neither is present.
    std::optional<std::int64_t> field_value() {
        if (accept('*')) {
            const auto value = small(argument().word());
            if (!value || *value < -MAX_FIELD || *value > MAX_FIELD) {
                bad_argument();
            }
            return value;
        }
        std::optional<std::int64_t> value;
        for (auto digit = peek(); digit && *digit >= '0' && *digit <= '9'; digit = peek()) {
            ++at_;
            value = value.value_or(0) * 10 + (*digit - '0');
            if (*value > MAX_FIELD) {
                bad_argument();
            }
        }
        return value;
    }

    // The pad character after a second '.': '*' takes it from the arguments when there is one.
    char32_t pad_character() {
        if (peek() != '*' || !arguments_.is_cons()) {
            return character();
        }
        ++at_;
        const auto value = small(argument().word());
        if (!value || !unicode_character(*value)) {
            bad_argument();
        }
        return static_cast<char32_t>(*value);
    }

    // Width, precision and pad: ~F.P.Pad; a negative width adjusts left.
    void fields(Control &control) {
        const bool minus = accept('-');
        control.width = field_value();
        if (minus && !control.width) {
            bad_argument();
        }
        const auto signed_width = minus ? -*control.width : control.width;
        control.left = signed_width && *signed_width < 0;
        control.width = signed_width ? std::optional{std::abs(*signed_width)} : std::nullopt;
        if (accept('.')) {
            control.precision = field_value();
            control.pad = accept('.') ? pad_character() : U' ';
        }
        if (control.precision && *control.precision < 0) {
            bad_argument();
        }
    }

    // The t, l and k modifiers; K (an ordering argument) is not supported.
    void modifiers(Control &control) {
        for (;;) {
            if (accept('t')) {
                control.unicode = true;
            } else if (accept('l')) {
                control.strings = false;
            } else if (!accept('k')) {
                return;
            }
        }
    }

    // One control sequence after its '~'.
    Control control() {
        Control result;
        fields(result);
        modifiers(result);
        result.letter = character();
        static constexpr std::u32string_view WITH_ARGUMENT = U"wpsbBci";
        if (WITH_ARGUMENT.contains(result.letter)) {
            result.argument = argument();
        } else if ((result.letter != '~' && result.letter != 'n') || !arguments_.is_list()) {
            bad_argument();
        }
        return result;
    }

    const std::vector<Piece> &pieces_;
    // Next unread piece.
    std::size_t at_ = 0;
    // Arguments not yet consumed.
    Term arguments_;
};

// `count` copies of `c`.
std::u32string repeated(char32_t c, std::int64_t count) { return std::u32string(static_cast<std::size_t>(count), c); }

// `data` padded on the right (left adjusted) or on the left.
std::u32string adjust(const std::u32string &data, const std::u32string &pad, bool left) {
    return left ? data + pad : pad + data;
}

// A character control (~c, ~~): F copies padded, or P copies within F.
std::u32string character_field(char32_t c, const Control &control) {
    const auto &[width, left, precision, pad, unicode, strings, letter, argument] = control;
    if (!width || !precision) {
        return repeated(c, width.value_or(precision.value_or(1)));
    }
    if (*width < *precision) {
        bad_argument();
    }
    return adjust(repeated(c, *precision), repeated(pad, *width - *precision), left);
}

// io_lib_format:term/6: text within the field width, or '*' characters when it does not fit.
std::u32string term_field(std::u32string text, const Control &control, std::optional<std::int64_t> precision) {
    const auto width = control.width ? control.width : precision;
    if (!width) {
        return text;
    }
    const auto length = static_cast<std::int64_t>(text.size());
    const auto room = std::min(length, precision ? std::min(*precision, *width) : *width);
    if (length > room) {
        return adjust(repeated('*', room), repeated(control.pad, *width - room), control.left);
    }
    return adjust(text, repeated(control.pad, *width - length), control.left);
}

// io_lib_format:string_field/6: truncated to, or padded up to, `width`.
std::u32string string_field(std::u32string text, std::int64_t width, bool left, char32_t pad) {
    const auto length = static_cast<std::int64_t>(text.size());
    if (length > width) {
        text.resize(static_cast<std::size_t>(width));
        return text;
    }
    return adjust(text, repeated(pad, width - length), left);
}

// io_lib_format:string/6: precision P characters (truncated or padded) within field width F.
std::u32string string_text(std::u32string text, const Control &control) {
    const auto &width = control.width;
    const auto &precision = control.precision;
    if (!width || !precision) {
        return width || precision ? string_field(std::move(text), width.value_or(precision.value_or(0)),
                                                 width ? control.left : true, control.pad)
                                  : text;
    }
    if (*width < *precision) {
        bad_argument();
    }
    if (*width == *precision) {
        return string_field(std::move(text), *width, control.left, control.pad);
    }
    text = string_field(std::move(text), *precision, true, control.pad);
    return adjust(text, repeated(control.pad, *width - *precision), control.left);
}

// ~s: an atom, or chardata (Latin-1 without t).
std::u32string string_control(const Control &control) {
    const auto &argument = *control.argument;
    auto text = argument.is_atom() ? code_points(need(argument.atom_spelling()))
                                   : chardata(argument, control.unicode ? Charset::lenient : Charset::latin1);
    if (!control.unicode && std::ranges::any_of(text, [](char32_t c) { return c > 0xFF; })) {
        bad_argument();
    }
    return string_text(std::move(text), control);
}

// ~b and ~B: an integer in base P (2..36, default 10), lowercase for ~b.
std::u32string integer_control(const Control &control) {
    const auto &argument = *control.argument;
    const auto base = control.precision.value_or(10);
    if (!argument.is_integer() || base < 2 || base > 36) {
        bad_argument();
    }
    auto digits = integer_digits(need(detail::integer_read(argument)), static_cast<unsigned>(base));
    if (control.letter == 'b') {
        std::ranges::transform(digits, digits.begin(), [](char c) { return c >= 'A' ? static_cast<char>(c + 32) : c; });
    }
    return term_field(code_points(digits), control, std::nullopt);
}

// ~c: a character, reduced to its low byte (two's complement) without t.
std::u32string char_control(const Control &control) {
    const auto &argument = *control.argument;
    if (!argument.is_integer()) {
        bad_argument();
    }
    const auto value = need(detail::integer_read(argument));
    if (!control.unicode) {
        const detail::Integer byte = (value % 256 + 256) % 256;
        return character_field(byte.convert_to<char32_t>(), control);
    }
    if (value < 0 || value > MAX_CODE_POINT || !unicode_character(value.convert_to<std::int64_t>())) {
        bad_argument();
    }
    return character_field(value.convert_to<char32_t>(), control);
}

// ~n: F newlines; a left adjusted width is badarg.
std::u32string newline_control(const Control &control) {
    if (control.width && control.left) {
        bad_argument();
    }
    return repeated('\n', control.width.value_or(1));
}

// ~w: io_lib:write/1 text in the field.
std::u32string write_control(const Control &control) {
    const auto style = control.unicode ? TermStyle::write_unicode : TermStyle::write;
    return term_field(code_points(need(format_term(*control.argument, style))), control, control.precision);
}

// ~p: pretty printed from the current column (or the precision) within a line of the field width (default 80).
std::u32string print_control(const Control &control, std::int64_t column) {
    if (control.left) {
        bad_argument();
    }
    const PrettyOptions options{control.precision.value_or(column + 1), control.width.value_or(80), control.unicode,
                                control.strings};
    return pretty(*control.argument, options);
}

// The text of one control sequence; `column` is where it starts.
std::u32string control_text(const Control &control, std::int64_t column) {
    switch (control.letter) {
    case 'w':
        return write_control(control);
    case 'p':
        return print_control(control, column);
    case 's':
        return string_control(control);
    case 'b':
    case 'B':
        return integer_control(control);
    case 'c':
        return char_control(control);
    case '~':
        return character_field('~', control);
    case 'n':
        return newline_control(control);
    case 'i':
        return {};
    default:
        bad_argument();
    }
}

// The column after `text` starting at `column`: newlines restart it, tabs advance to the next multiple of 8.
std::int64_t next_column(std::u32string_view text, std::int64_t column) {
    for (const auto c : text) {
        column = c == '\n' ? 0 : c == '\t' ? (column + 8) / 8 * 8 : column + 1;
    }
    return column;
}

// Append one element and return the next column.
std::int64_t build(const Element &element, std::int64_t column, std::u32string &out) {
    if (const auto *control = std::get_if<Control>(&element)) {
        const auto text = control_text(*control, column);
        out += text;
        return next_column(text, column);
    }
    const auto &piece = std::get<Piece>(element);
    out += piece.chunk ? *piece.chunk : std::u32string(1, piece.code);
    return piece.chunk ? column + 1 : next_column(std::u32string_view(&piece.code, 1), column);
}
} // namespace

std::u32string characters(const Term &chardata_term) { return chardata(chardata_term, Charset::unicode); }

std::u32string format_text(const Term &format, std::optional<Term> arguments) {
    const auto pieces = format_pieces(format);
    const auto elements =
        Scanner(pieces, arguments ? std::move(*arguments) : Term::from_word(abi::v1::empty_list).value()).run();
    std::u32string out;
    std::int64_t column = 0;
    for (const auto &element : elements) {
        column = build(element, column, out);
    }
    if (!std::ranges::all_of(out, [](char32_t c) { return unicode_character(c); })) {
        bad_argument();
    }
    return out;
}
} // namespace erlang_aot::runtime::builtins
