#pragma once
// External Term Format (term_to_binary/1, binary_to_term/1, distribution messages): tags, layouts and bounds checks
// shared by the compiler (parse transform loader exchange) and the runtime. Independent of any term representation:
// encoders walk their own terms and drive Writer; decoders pull Items from Reader and build their own terms.
// Added for parse transforms.
#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace clause::abi::external {
inline constexpr std::uint8_t VERSION = 131;

// Tags this codec reads; Writer emits the ones term_to_binary/1 chooses since OTP 26.
enum Tag : std::uint8_t {
    NEW_FLOAT = 70,
    BIT_BINARY = 77,
    SMALL_INTEGER = 97,
    INTEGER = 98,
    FLOAT_TEXT = 99,
    ATOM_LATIN1 = 100,
    SMALL_TUPLE = 104,
    LARGE_TUPLE = 105,
    NIL = 106,
    STRING = 107,
    LIST = 108,
    BINARY = 109,
    SMALL_BIG = 110,
    LARGE_BIG = 111,
    SMALL_ATOM_LATIN1 = 115,
    MAP = 116,
    ATOM_UTF8 = 118,
    SMALL_ATOM_UTF8 = 119
};

// Largest counts of the compact encodings.
inline constexpr std::size_t SMALL_LIMIT = 255;
inline constexpr std::size_t STRING_LIMIT = 65535;

// Malformed, truncated or unsupported encoded data.
class FormatError : public std::runtime_error {
  public:
    explicit FormatError(const std::string &message) : std::runtime_error(message) {}
};

enum class ItemKind : std::uint8_t { atom, integer, big_integer, floating, tuple, list, nil, string, binary, map };

// One decoded tag. Container items announce how many items follow: a tuple's elements, a list's elements and then
// its tail, a map's keys and values alternating.
struct Item {
    ItemKind kind_ = ItemKind::nil;
    // Atom name (UTF-8, or Latin-1 when latin1_), STRING_EXT bytes, binary bytes, or a big integer's little-endian
    // magnitude; views into the decoded input.
    std::string_view bytes_;
    bool latin1_ = false;
    // Sign of a big integer, value of an integer that has no big encoding, value of a float.
    bool negative_ = false;
    std::int64_t integer_ = 0;
    double float_ = 0;
    // Tuple arity, list element count (excluding the tail), map pair count, binary bit count.
    std::size_t count_ = 0;
};

// Appends encoded tags to a byte string; callers emit children in order after a container header.
class Writer {
  public:
    explicit Writer(std::string &out) : out_(out) {}

    void version() { out_.push_back(static_cast<char>(VERSION)); }

    // An atom spelled in UTF-8 (at most 255 characters, so at most 1020 bytes).
    void atom(const std::string_view utf8) {
        if (utf8.size() <= SMALL_LIMIT) {
            tag(SMALL_ATOM_UTF8);
            put<1>(utf8.size());
        } else {
            tag(ATOM_UTF8);
            put<2>(utf8.size());
        }
        out_ += utf8;
    }

    // An integer of at most 64 bits, in the smallest encoding.
    void integer(const std::int64_t value) {
        if (value >= 0 && value <= static_cast<std::int64_t>(SMALL_LIMIT)) {
            tag(SMALL_INTEGER);
            put<1>(static_cast<std::uint64_t>(value));
        } else if (value >= std::numeric_limits<std::int32_t>::min() &&
                   value <= std::numeric_limits<std::int32_t>::max()) {
            tag(INTEGER);
            put<4>(static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
        } else {
            auto magnitude = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
            std::string bytes;
            for (; magnitude != 0; magnitude >>= 8U) {
                bytes.push_back(static_cast<char>(magnitude & 0xFFU));
            }
            big_integer(value < 0, bytes);
        }
    }

    // An integer outside the 32-bit range from its little-endian magnitude bytes without trailing zero bytes.
    void big_integer(const bool negative, const std::string_view magnitude) {
        if (magnitude.size() <= SMALL_LIMIT) {
            tag(SMALL_BIG);
            put<1>(magnitude.size());
        } else {
            tag(LARGE_BIG);
            put<4>(magnitude.size());
        }
        out_.push_back(static_cast<char>(negative ? 1 : 0));
        out_ += magnitude;
    }

    void floating(const double value) {
        tag(NEW_FLOAT);
        put<8>(std::bit_cast<std::uint64_t>(value));
    }

    // Container headers; `length` elements and then the tail follow a list header.
    void tuple(const std::size_t arity) {
        if (arity <= SMALL_LIMIT) {
            tag(SMALL_TUPLE);
            put<1>(arity);
        } else {
            tag(LARGE_TUPLE);
            put<4>(arity);
        }
    }

    void list(const std::size_t length) {
        tag(LIST);
        put<4>(length);
    }

    void map(const std::size_t pairs) {
        tag(MAP);
        put<4>(pairs);
    }

    void nil() { tag(NIL); }

    // A proper list of 1..65535 byte values.
    void string(const std::string_view bytes) {
        tag(STRING);
        put<2>(bytes.size());
        out_ += bytes;
    }

    // A binary, or a bitstring whose last byte holds its final bits in the high positions.
    void binary(const std::string_view bytes, const std::size_t bit_count) {
        const auto tail_bits = bit_count % 8;
        tag(tail_bits == 0 ? BINARY : BIT_BINARY);
        put<4>(bytes.size());
        if (tail_bits != 0) {
            put<1>(tail_bits);
        }
        out_ += bytes;
    }

  private:
    // The output being appended to.
    std::string &out_;

    void tag(const Tag value) { out_.push_back(static_cast<char>(value)); }

    // Append a big-endian unsigned value of BYTES bytes.
    template <int BYTES> void put(const std::uint64_t value) {
        for (int shift = (BYTES - 1) * 8; shift >= 0; shift -= 8) {
            out_.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
        }
    }
};

// Pulls one tag at a time from encoded bytes, checking every length against the remaining input.
class Reader {
  public:
    explicit Reader(const std::string_view bytes) : bytes_(bytes) {}

    // Consume the version byte that starts every encoding.
    void version() {
        if (take<1>() != VERSION) {
            fail("missing version 131");
        }
    }

    bool done() const { return at_ == bytes_.size(); }

    std::size_t offset() const { return at_; }

    // Decode the next tag.
    Item next() {
        const auto tag = take<1>();
        switch (tag) {
        case SMALL_TUPLE:
            return container(ItemKind::tuple, take<1>(), 1);
        case LARGE_TUPLE:
            return container(ItemKind::tuple, take<4>(), 1);
        case LIST:
            return container(ItemKind::list, take<4>(), 1);
        case MAP:
            return container(ItemKind::map, take<4>(), 2);
        case NIL:
            return item(ItemKind::nil);
        case STRING:
            return with_bytes(ItemKind::string, take_bytes(take<2>()));
        default:
            return number(tag);
        }
    }

    [[noreturn]] void fail(const std::string &message) const {
        throw FormatError("external term format: " + message + " at byte " + std::to_string(at_));
    }

  private:
    // Input and read position.
    std::string_view bytes_;
    std::size_t at_ = 0;

    // Items of each shape, with every other field left at its default.
    static Item item(const ItemKind kind) {
        Item result;
        result.kind_ = kind;
        return result;
    }

    static Item with_bytes(const ItemKind kind, const std::string_view bytes) {
        auto result = item(kind);
        result.bytes_ = bytes;
        return result;
    }

    static Item latin1(const std::string_view name) {
        auto result = with_bytes(ItemKind::atom, name);
        result.latin1_ = true;
        return result;
    }

    static Item integer(const std::int64_t value) {
        auto result = item(ItemKind::integer);
        result.integer_ = value;
        return result;
    }

    static Item floating(const double value) {
        auto result = item(ItemKind::floating);
        result.float_ = value;
        return result;
    }

    // Read a big-endian unsigned value of BYTES bytes.
    template <int BYTES> std::uint64_t take() {
        if (bytes_.size() - at_ < static_cast<std::size_t>(BYTES)) {
            fail("truncated data");
        }
        std::uint64_t value = 0;
        for (int index = 0; index < BYTES; ++index) {
            value = (value << 8U) | static_cast<std::uint8_t>(bytes_[at_++]);
        }
        return value;
    }

    std::string_view take_bytes(const std::uint64_t count) {
        if (bytes_.size() - at_ < count) {
            fail("truncated data");
        }
        const auto result = bytes_.substr(at_, static_cast<std::size_t>(count));
        at_ += static_cast<std::size_t>(count);
        return result;
    }

    // A container header; every child needs at least one more byte, so larger counts are malformed.
    Item container(const ItemKind kind, const std::uint64_t count, const std::uint64_t children_per_count) {
        if (count > (bytes_.size() - at_) / children_per_count) {
            fail("container larger than its input");
        }
        auto result = item(kind);
        result.count_ = static_cast<std::size_t>(count);
        return result;
    }

    // Integers and floats.
    Item number(const std::uint64_t tag) {
        switch (tag) {
        case SMALL_INTEGER:
            return integer(static_cast<std::int64_t>(take<1>()));
        case INTEGER:
            return integer(static_cast<std::int32_t>(take<4>()));
        case SMALL_BIG:
            return big(take<1>());
        case LARGE_BIG:
            return big(take<4>());
        case NEW_FLOAT:
            return floating(std::bit_cast<double>(take<8>()));
        case FLOAT_TEXT:
            return float_text();
        default:
            return text(tag);
        }
    }

    // Atoms and binaries.
    Item text(const std::uint64_t tag) {
        switch (tag) {
        case SMALL_ATOM_UTF8:
            return with_bytes(ItemKind::atom, take_bytes(take<1>()));
        case ATOM_UTF8:
            return with_bytes(ItemKind::atom, take_bytes(take<2>()));
        case SMALL_ATOM_LATIN1:
            return latin1(take_bytes(take<1>()));
        case ATOM_LATIN1:
            return latin1(take_bytes(take<2>()));
        case BINARY: {
            auto result = with_bytes(ItemKind::binary, take_bytes(take<4>()));
            result.count_ = result.bytes_.size() * 8;
            return result;
        }
        case BIT_BINARY:
            return bit_binary();
        default:
            fail("unsupported tag " + std::to_string(tag));
        }
    }

    Item big(const std::uint64_t size) {
        const bool negative = take<1>() != 0;
        auto result = with_bytes(ItemKind::big_integer, take_bytes(size));
        result.negative_ = negative;
        return result;
    }

    Item bit_binary() {
        const auto size = take<4>();
        const auto tail_bits = take<1>();
        if (size == 0 || tail_bits == 0 || tail_bits > 7) {
            fail("invalid bitstring size");
        }
        auto result = with_bytes(ItemKind::binary, take_bytes(size));
        result.count_ = static_cast<std::size_t>((size - 1) * 8 + tail_bits);
        return result;
    }

    // The old 31-byte textual float encoding.
    Item float_text() {
        constexpr std::size_t SIZE = 31;
        const auto text = take_bytes(SIZE);
        const auto end = text.find('\0');
        const auto digits = text.substr(0, end);
        double value = 0;
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (parsed.ec != std::errc{}) {
            fail("invalid float text");
        }
        return floating(value);
    }
};
} // namespace clause::abi::external
