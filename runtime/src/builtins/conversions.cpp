#include "../terms/integers.hpp"
#include "float_text.hpp"
#include "support.hpp"
#include "terms.hpp"
#include <algorithm>
#include <array>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <vector>

// The conversion builtins (docs/builtins.md): atoms, integers, floats, lists and binaries, with OTP's badarg and
// system_limit rules. TODO(step 43A): conversions of long inputs should run in bounded portions.
namespace erlang_aot::runtime::builtins {
namespace {
using detail::Integer;

// Largest Unicode code point; surrogates are not characters.
constexpr std::int64_t MAX_CODE_POINT = 0x10FFFF;
// Atoms hold at most 255 characters.
constexpr std::size_t MAX_ATOM_CHARACTERS = 255;
// Digits of integer_to_list/2, uppercase like OTP's.
constexpr std::string_view DIGITS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// A proper list of the given character codes.
Word char_list(ProcessContext &context, std::span<const std::uint32_t> codes) {
    std::vector<Term> items;
    items.reserve(codes.size());
    for (const auto code : codes) {
        items.push_back(*Term::from_word(*abi::v1::NativeIntegerEncoding::encode(code)));
    }
    return publish(context, TermFactory(context).list(items));
}

// A proper list of the bytes of `text`.
Word byte_list(ProcessContext &context, std::string_view text) {
    std::vector<std::uint32_t> codes(text.begin(), text.end());
    std::ranges::transform(text, codes.begin(), [](char c) { return static_cast<unsigned char>(c); });
    return char_list(context, codes);
}

// The byte count of a UTF-8 sequence from its lead byte.
std::size_t sequence_length(unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    return lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4);
}

// The code points of valid UTF-8 text (atom spellings are validated when interned).
std::vector<std::uint32_t> code_points(std::string_view text) {
    std::vector<std::uint32_t> result;
    for (std::size_t at = 0; at < text.size();) {
        const auto lead = static_cast<unsigned char>(text[at]);
        const auto length = sequence_length(lead);
        std::uint32_t code = length == 1 ? lead : lead & (0x7FU >> length);
        for (const char byte : text.substr(at + 1, length - 1)) {
            code = (code << 6) | (static_cast<unsigned char>(byte) & 0x3FU);
        }
        result.push_back(code);
        at += length;
    }
    return result;
}

// Append `code` to `text` as UTF-8.
void encode(std::uint32_t code, std::string &text) {
    if (code < 0x80) {
        text.push_back(static_cast<char>(code));
        return;
    }
    const std::size_t length = code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
    static constexpr std::array<unsigned, 5> LEADS{0, 0, 0xC0, 0xE0, 0xF0};
    text.push_back(static_cast<char>(LEADS.at(length) | (code >> (6 * (length - 1)))));
    for (std::size_t i = length - 1; i > 0; --i) {
        text.push_back(static_cast<char>(0x80U | ((code >> (6 * (i - 1))) & 0x3FU)));
    }
}

// The head of a cons, advancing `list` to its tail.
Term next(Term &list) {
    auto head = list.head().value();
    list = list.tail().value();
    return head;
}

// atom_to_list(Atom): its characters.
Word atom_to_list(ProcessContext &context, Arguments arguments) {
    const auto atom = admit(context, arguments[0]);
    if (!atom) {
        return 0;
    }
    const auto spelling = atom->is_atom() ? atom->atom_spelling() : std::unexpected(TermError::wrong_type);
    return spelling ? char_list(context, code_points(*spelling)) : badarg(context);
}

// Why a list_to_atom argument is not a name: OTP checks the length before each character.
enum class NameError : std::uint8_t { badarg, too_long };

// The UTF-8 spelling of a proper list of at most 255 characters.
std::expected<std::string, NameError> atom_name(Term list) {
    std::string text;
    for (std::size_t count = 0; !list.is_nil(); ++count) {
        if (!list.is_cons()) {
            return std::unexpected(NameError::badarg);
        }
        if (count == MAX_ATOM_CHARACTERS) {
            return std::unexpected(NameError::too_long);
        }
        const auto code = small(next(list).word());
        if (!code || *code < 0 || *code > MAX_CODE_POINT || (*code >= 0xD800 && *code <= 0xDFFF)) {
            return std::unexpected(NameError::badarg);
        }
        encode(static_cast<std::uint32_t>(*code), text);
    }
    return text;
}

// list_to_atom(String): more than 255 characters is system_limit; a full atom table stops the program.
Word list_to_atom(ProcessContext &context, Arguments arguments) {
    const auto list = admit(context, arguments[0]);
    if (!list) {
        return 0;
    }
    const auto name = atom_name(*list);
    if (!name) {
        return name.error() == NameError::too_long ? raise(context, abi::v1::ErrorReason::system_limit)
                                                   : badarg(context);
    }
    return publish(context, context.atom_storage().intern(*name));
}

// A base argument: a small integer in 2..36.
std::optional<unsigned> base_of(Word word) {
    const auto base = small(word);
    return base && *base >= 2 && *base <= 36 ? std::optional{static_cast<unsigned>(*base)} : std::nullopt;
}

// The largest power of `base` used as a chunk of digits, and its digit count.
std::pair<std::uint64_t, unsigned> chunk(unsigned base) {
    std::uint64_t power = base;
    unsigned digits = 1;
    while (power <= (std::uint64_t{1} << 63) / base) {
        power *= base;
        ++digits;
    }
    return {power, digits};
}

// The digits of `value` in `base` (2..36), most significant first, with a leading '-' when negative.
std::string integer_digits(const Integer &value, unsigned base) {
    if (base == 10) {
        return detail::integer_text(value);
    }
    const auto [power, width] = chunk(base);
    Integer magnitude = boost::multiprecision::abs(value);
    std::string reversed;
    for (;;) {
        Integer quotient;
        Integer remainder;
        boost::multiprecision::divide_qr(magnitude, Integer(power), quotient, remainder);
        auto part = remainder.convert_to<std::uint64_t>();
        for (unsigned i = 0; i < width && (quotient != 0 || part != 0 || i == 0); ++i) {
            reversed.push_back(DIGITS[part % base]);
            part /= base;
        }
        if (quotient == 0) {
            break;
        }
        magnitude.swap(quotient);
    }
    if (value < 0) {
        reversed.push_back('-');
    }
    return {reversed.rbegin(), reversed.rend()};
}

// integer_to_list(Integer) and integer_to_list(Integer, Base).
Word integer_to_list(ProcessContext &context, Arguments arguments) {
    const auto number = admit(context, arguments[0]);
    if (!number) {
        return 0;
    }
    const auto base = arguments.size() == 1 ? std::optional{10U} : base_of(arguments[1]);
    const auto value = number->is_integer() ? detail::integer_read(*number) : std::unexpected(TermError::wrong_type);
    if (!value || !base) {
        return badarg(context);
    }
    return byte_list(context, integer_digits(*value, *base));
}

// The value of a digit character of base 36 (either letter case); 36 for any other character.
unsigned digit_of(unsigned char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<unsigned>(character - '0');
    }
    if (character >= 'a' && character <= 'z') {
        return static_cast<unsigned>(character - 'a' + 10);
    }
    return character >= 'A' && character <= 'Z' ? static_cast<unsigned>(character - 'A' + 10) : 36;
}

// Accumulates digits in machine-word chunks before folding them into the multiprecision value.
class DigitReader {
  public:
    explicit DigitReader(unsigned base) : base_(base) {}

    // Add one digit value.
    void add(unsigned digit) {
        part_ = part_ * base_ + digit;
        scale_ *= base_;
        ++count_;
        if (scale_ > (std::uint64_t{1} << 63) / base_) {
            flush();
        }
    }

    // The value read so far; none when no digit was read.
    std::optional<Integer> value() {
        flush();
        return count_ ? std::optional{value_} : std::nullopt;
    }

  private:
    // Fold the pending chunk into the value.
    void flush() {
        value_ = value_ * Integer(scale_) + Integer(part_);
        part_ = 0;
        scale_ = 1;
    }

    unsigned base_;
    // The value of the folded digits, the pending chunk, its scale and the digit count.
    Integer value_ = 0;
    std::uint64_t part_ = 0;
    std::uint64_t scale_ = 1;
    std::size_t count_ = 0;
};

// The characters of a proper list of bytes; none for anything else.
std::optional<std::vector<unsigned char>> byte_codes(Term list) {
    std::vector<unsigned char> codes;
    while (list.is_cons()) {
        const auto code = small(next(list).word());
        if (!code || *code < 0 || *code > 255) {
            return std::nullopt;
        }
        codes.push_back(static_cast<unsigned char>(*code));
    }
    return list.is_nil() ? std::optional{std::move(codes)} : std::nullopt;
}

// Digits OTP reads into a small integer before switching to its multiprecision path.
std::size_t small_digits(unsigned base) {
    std::size_t digits = 0;
    for (std::uint64_t power = base; power < (std::uint64_t{1} << 59); power *= base) {
        ++digits;
    }
    return digits;
}

// OTP's size limits for its multiprecision path, checked before the digits: system_limit past them.
bool too_many_digits(std::span<const unsigned char> digits, unsigned base) {
    const auto small = small_digits(base);
    const bool multiprecision =
        digits.size() > small &&
        std::ranges::all_of(digits.first(small + 1), [&](unsigned char c) { return digit_of(c) < base; });
    return multiprecision && (digits.size() > 4'194'304 || (digits.size() > 1'262'611 && base >= 10));
}

// Why a list_to_integer argument is not an integer.
enum class ParseError : std::uint8_t { badarg, system_limit };

// The digits of `text` after its optional sign, with OTP's leading-zero and size rules.
std::expected<Integer, ParseError> parse_digits(std::span<const unsigned char> text, unsigned base) {
    const auto zeros =
        static_cast<std::size_t>(std::ranges::find_if(text, [](auto c) { return c != '0'; }) - text.begin());
    const auto digits = text.subspan(zeros);
    if (too_many_digits(digits, base)) {
        return std::unexpected(ParseError::system_limit);
    }
    DigitReader reader(base);
    for (const auto character : digits) {
        if (digit_of(character) >= base) {
            return std::unexpected(ParseError::badarg);
        }
        reader.add(digit_of(character));
    }
    if (zeros == 0 && digits.empty()) {
        return std::unexpected(ParseError::badarg);
    }
    return reader.value().value_or(Integer(0));
}

// An optional sign and at least one digit of `base`.
std::expected<Integer, ParseError> parse_integer(std::span<const unsigned char> text, unsigned base) {
    const bool signed_text = !text.empty() && (text.front() == '-' || text.front() == '+');
    auto value = parse_digits(signed_text ? text.subspan(1) : text, base);
    if (value && signed_text && text.front() == '-') {
        *value = -*value;
    }
    return value;
}

// list_to_integer(String) and list_to_integer(String, Base); a value past the integer limit is system_limit.
Word list_to_integer(ProcessContext &context, Arguments arguments) {
    const auto list = admit(context, arguments[0]);
    if (!list) {
        return 0;
    }
    const auto base = arguments.size() == 1 ? std::optional{10U} : base_of(arguments[1]);
    const auto text = base ? byte_codes(*list) : std::nullopt;
    const auto value = text ? parse_integer(*text, *base) : std::unexpected(ParseError::badarg);
    if (!value || detail::integer_bits(*value) > detail::integer_bit_limit) {
        return value.error_or(ParseError::system_limit) == ParseError::badarg
                   ? badarg(context)
                   : raise(context, abi::v1::ErrorReason::system_limit);
    }
    return publish(context, detail::IntegerAccess::make(context.heap(), *value));
}

// Apply the atom option compact or short; false for any other atom.
bool float_flag(std::string_view name, FloatFormat &format) {
    if (name == "compact") {
        format.compact = true;
        return true;
    }
    if (name == "short") {
        format.kind = FloatFormat::Kind::shortest;
        return true;
    }
    return false;
}

// Apply one float_to_list/2 option; false for anything OTP rejects.
bool float_option(const Term &option, FloatFormat &format) {
    if (option.is_atom()) {
        const auto name = option.atom_spelling();
        return name && float_flag(*name, format);
    }
    if (!option.is_tuple() || option.tuple_size().value_or(0) != 2) {
        return false;
    }
    const auto kind = option.tuple_element(0).value().atom_spelling();
    const auto digits = small(option.tuple_element(1).value().word());
    if (!kind || !digits || (*kind != "decimals" && *kind != "scientific")) {
        return false;
    }
    format.kind = *kind == "decimals" ? FloatFormat::Kind::fixed : FloatFormat::Kind::scientific;
    format.decimals = *digits;
    return true;
}

// Apply a proper list of options in order; false when one is invalid or the list is improper.
bool float_options(Term options, FloatFormat &format) {
    while (options.is_cons()) {
        if (!float_option(next(options), format)) {
            return false;
        }
    }
    return options.is_nil();
}

// float_to_list(Float) and float_to_list(Float, Options): later options override earlier ones.
Word float_to_list(ProcessContext &context, Arguments arguments) {
    const auto number = admit(context, arguments[0]);
    auto options = arguments.size() == 1 ? Term::from_word(abi::v1::empty_list).value() : admit(context, arguments[1]);
    if (!number || !options) {
        return 0;
    }
    FloatFormat format;
    const auto text = number->is_float() && float_options(*options, format)
                          ? float_text(number->float_value().value(), format)
                          : std::nullopt;
    return text ? byte_list(context, *text) : badarg(context);
}

// binary_to_list(Binary): its bytes; other bitstrings are badarg.
Word binary_to_list(ProcessContext &context, Arguments arguments) {
    const auto binary = admit(context, arguments[0]);
    if (!binary) {
        return 0;
    }
    const auto bytes = binary->is_binary() ? binary->binary_bytes() : std::unexpected(TermError::wrong_type);
    if (!bytes) {
        return badarg(context);
    }
    std::vector<std::uint32_t> codes;
    codes.reserve(bytes->size());
    std::ranges::transform(*bytes, std::back_inserter(codes), [](std::byte b) { return std::to_integer<unsigned>(b); });
    return char_list(context, codes);
}

// Append the bytes of a binary; false for any other term.
bool append_binary(const Term &term, std::vector<std::byte> &bytes) {
    const auto content = term.is_binary() ? term.binary_bytes() : std::unexpected(TermError::wrong_type);
    if (content) {
        bytes.insert(bytes.end(), content->begin(), content->end());
    }
    return content.has_value();
}

// Append one iolist byte or binary; false for any other term.
bool append_leaf(const Term &element, std::vector<std::byte> &bytes) {
    const auto value = small(element.word());
    if (value && *value >= 0 && *value <= 255) {
        bytes.push_back(static_cast<std::byte>(*value));
        return true;
    }
    return append_binary(element, bytes);
}

// Read one list until its end or a nested list, which is queued to be read before the rest of this one.
bool read_list(Term list, std::vector<std::byte> &bytes, std::vector<Term> &pending) {
    while (list.is_cons()) {
        auto element = next(list);
        if (element.is_nil() || element.is_cons()) {
            pending.push_back(std::move(list));
            pending.push_back(std::move(element));
            return true;
        }
        if (!append_leaf(element, bytes)) {
            return false;
        }
    }
    return list.is_nil() || append_binary(list, bytes);
}

// The bytes of an iolist: bytes, binaries and nested iolists, each list ending in [] or a binary.
std::optional<std::vector<std::byte>> iolist_bytes(const Term &root) {
    std::vector<std::byte> bytes;
    std::vector<Term> pending{root};
    while (!pending.empty()) {
        auto list = std::move(pending.back());
        pending.pop_back();
        if (!read_list(std::move(list), bytes, pending)) {
            return std::nullopt;
        }
    }
    return bytes;
}

// list_to_binary(IoList) and iolist_to_binary(IoListOrBinary).
Word list_to_binary(ProcessContext &context, Arguments arguments, bool binary_allowed) {
    const auto root = admit(context, arguments[0]);
    if (!root) {
        return 0;
    }
    if (binary_allowed && root->is_binary()) {
        return root->word();
    }
    const auto bytes = root->is_nil() || root->is_cons() ? iolist_bytes(*root) : std::nullopt;
    return bytes ? publish(context, TermFactory(context).binary(*bytes)) : badarg(context);
}

// list_to_binary(IoList): the argument must be a list.
Word list_to_binary1(ProcessContext &context, Arguments arguments) { return list_to_binary(context, arguments, false); }

// iolist_to_binary(IoListOrBinary): a binary is returned as it is.
Word iolist_to_binary(ProcessContext &context, Arguments arguments) { return list_to_binary(context, arguments, true); }

constexpr std::array CONVERSION_BUILTINS{
    BuiltinEntry{"erlang", "atom_to_list", 1, atom_to_list},
    BuiltinEntry{"erlang", "list_to_atom", 1, list_to_atom},
    BuiltinEntry{"erlang", "integer_to_list", 1, integer_to_list},
    BuiltinEntry{"erlang", "integer_to_list", 2, integer_to_list},
    BuiltinEntry{"erlang", "list_to_integer", 1, list_to_integer},
    BuiltinEntry{"erlang", "list_to_integer", 2, list_to_integer},
    BuiltinEntry{"erlang", "float_to_list", 1, float_to_list},
    BuiltinEntry{"erlang", "float_to_list", 2, float_to_list},
    BuiltinEntry{"erlang", "binary_to_list", 1, binary_to_list},
    BuiltinEntry{"erlang", "list_to_binary", 1, list_to_binary1},
    BuiltinEntry{"erlang", "iolist_to_binary", 1, iolist_to_binary},
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> conversion_builtins() noexcept { return builtins::CONVERSION_BUILTINS; }
} // namespace erlang_aot::runtime
