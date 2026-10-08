#include "../terms/integers.hpp"
#include "float_text.hpp"
#include "terms.hpp"
#include "text.hpp"
#include "typed.hpp"
#include <algorithm>
#include <array>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <vector>

// The conversion builtins (docs/builtins.md): atoms, integers, floats, lists and binaries, with OTP's badarg and
// system_limit rules. TODO(step 43A): conversions of long inputs should run in bounded portions.
namespace erlang_aot::runtime::builtins {
namespace {
using detail::Integer;

// Atoms hold at most 255 characters.
constexpr std::size_t MAX_ATOM_CHARACTERS = 255;

// A proper list of the given character codes.
TermResult<Term> char_list(ProcessContext &context, std::u32string_view codes) {
    std::vector<Term> items;
    items.reserve(codes.size());
    for (const auto code : codes) {
        items.push_back(*Term::from_word(*abi::v1::NativeIntegerEncoding::encode(code)));
    }
    return TermFactory(context).list(items);
}

// A proper list of the bytes of `text`.
TermResult<Term> byte_list(ProcessContext &context, std::string_view text) {
    std::u32string codes(text.size(), 0);
    std::ranges::transform(text, codes.begin(), [](char c) { return static_cast<unsigned char>(c); });
    return char_list(context, codes);
}

// The head of a cons, advancing `list` to its tail.
Term next(Term &list) {
    auto head = list.head().value();
    list = list.tail().value();
    return head;
}

// atom_to_list(Atom): its characters.
TermResult<Term> atom_to_list(ProcessContext &context, const AtomArgument &atom) {
    return char_list(context, code_points(atom.spelling));
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
        if (!code || !unicode_character(*code)) {
            return std::unexpected(NameError::badarg);
        }
        encode(static_cast<char32_t>(*code), text);
    }
    return text;
}

// list_to_atom(String): more than 255 characters is system_limit; a full atom table stops the program.
BuiltinResult<Term> list_to_atom(ProcessContext &context, const Term &list) {
    const auto name = atom_name(list);
    if (!name) {
        return std::unexpected(BuiltinFailure{name.error() == NameError::too_long ? abi::v1::ErrorReason::system_limit
                                                                                  : abi::v1::ErrorReason::badarg});
    }
    return context.atom_storage().intern(*name).transform_error(
        [](TermError error) { return BuiltinFailure{.term = error}; });
}

// A base argument: a small integer in 2..36, else badarg.
unsigned base_of(std::int64_t base) {
    if (base < 2 || base > 36) {
        bad_argument();
    }
    return static_cast<unsigned>(base);
}

// integer_to_list(Integer, Base).
TermResult<Term> integer_to_list2(ProcessContext &context, const Integer &value, std::int64_t base) {
    return byte_list(context, integer_digits(value, base_of(base)));
}

// integer_to_list(Integer).
TermResult<Term> integer_to_list1(ProcessContext &context, const Integer &value) {
    return integer_to_list2(context, value, 10);
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

// list_to_integer(String, Base); a value past the integer limit is system_limit.
TermResult<Term> list_to_integer2(ProcessContext &context, const Term &list, std::int64_t base) {
    const auto radix = base_of(base);
    const auto text = byte_codes(list);
    const auto value = text ? parse_integer(*text, radix) : std::unexpected(ParseError::badarg);
    if (!value || detail::integer_bits(*value) > detail::integer_bit_limit) {
        throw BuiltinFailure{value.error_or(ParseError::system_limit) == ParseError::badarg
                                 ? abi::v1::ErrorReason::badarg
                                 : abi::v1::ErrorReason::system_limit};
    }
    return detail::IntegerAccess::make(context.heap(), *value);
}

// list_to_integer(String).
TermResult<Term> list_to_integer1(ProcessContext &context, const Term &list) {
    return list_to_integer2(context, list, 10);
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

// float_to_list(Float, Options): later options override earlier ones.
TermResult<Term> float_to_list2(ProcessContext &context, double number, const Term &options) {
    FloatFormat format;
    const auto text = float_options(options, format) ? float_text(number, format) : std::nullopt;
    if (!text) {
        bad_argument();
    }
    return byte_list(context, *text);
}

// float_to_list(Float).
TermResult<Term> float_to_list1(ProcessContext &context, double number) {
    return float_to_list2(context, number, Term::from_word(abi::v1::empty_list).value());
}

// binary_to_list(Binary): its bytes; other bitstrings are badarg.
TermResult<Term> binary_to_list(ProcessContext &context, const BinaryArgument &binary) {
    std::u32string codes;
    codes.reserve(binary.bytes.size());
    std::ranges::transform(binary.bytes, std::back_inserter(codes),
                           [](std::byte b) { return std::to_integer<char32_t>(b); });
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

// list_to_binary(IoList): the argument must be a list.
TermResult<Term> list_to_binary(ProcessContext &context, const Term &root) {
    const auto bytes = root.is_list() ? iolist_bytes(root) : std::nullopt;
    if (!bytes) {
        bad_argument();
    }
    return TermFactory(context).binary(*bytes);
}

// iolist_to_binary(IoListOrBinary): a binary is returned as it is.
TermResult<Term> iolist_to_binary(ProcessContext &context, const Term &root) {
    return root.is_binary() ? root : list_to_binary(context, root);
}

// The ~w text of an identity as a list: pid_to_list/1 accepts only pids, ref_to_list/1 only references.
template <bool Pid> TermResult<Term> identity_to_list(ProcessContext &context, const Term &value) {
    if (Pid ? !value.is_pid() : !value.is_reference()) {
        bad_argument();
    }
    return byte_list(context, need(format_term(value, TermStyle::write)));
}

constexpr std::array CONVERSION_BUILTINS{
    typed_entry<atom_to_list>("erlang", "atom_to_list"),
    typed_entry<list_to_atom>("erlang", "list_to_atom"),
    typed_entry<integer_to_list1>("erlang", "integer_to_list"),
    typed_entry<integer_to_list2>("erlang", "integer_to_list"),
    typed_entry<list_to_integer1>("erlang", "list_to_integer"),
    typed_entry<list_to_integer2>("erlang", "list_to_integer"),
    typed_entry<float_to_list1>("erlang", "float_to_list"),
    typed_entry<float_to_list2>("erlang", "float_to_list"),
    typed_entry<binary_to_list>("erlang", "binary_to_list"),
    typed_entry<list_to_binary>("erlang", "list_to_binary"),
    typed_entry<iolist_to_binary>("erlang", "iolist_to_binary"),
    typed_entry<identity_to_list<true>>("erlang", "pid_to_list"),
    typed_entry<identity_to_list<false>>("erlang", "ref_to_list"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> conversion_builtins() noexcept { return builtins::CONVERSION_BUILTINS; }
} // namespace erlang_aot::runtime
