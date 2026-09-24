#include <array>
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace erlang_aot::runtime;
using Encoding = erlang_aot::abi::v1::NativeIntegerEncoding;

// Keep behavior assertions active in optimized runtime-only builds too.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Compare the runtime boundary with the shared codec at signed boundaries and representative values.
void check_integers() {
    constexpr std::array values{Encoding::minimum, Encoding::minimum + 1, std::int64_t{-42},
                                std::int64_t{-1},  std::int64_t{0},       std::int64_t{1},
                                std::int64_t{42},  Encoding::maximum - 1, Encoding::maximum};
    for (const auto value : values) {
        const auto encoded = encode_integer(value);
        require(encoded && *encoded == *Encoding::encode(value), "runtime/ABI encoding mismatch");
        require(classify_immediate(*encoded) == TermKind::smallint, "integer misclassified");
        require(decode_integer(*encoded) == value, "integer round trip failed");
    }
    for (const auto value : {std::numeric_limits<std::int64_t>::min(), Encoding::minimum - 1, Encoding::maximum + 1,
                             std::numeric_limits<std::int64_t>::max()}) {
        require(encode_integer(value) == std::unexpected(TermError::out_of_range), "integer overflow accepted");
    }
}

// Reserved identities classify structurally without interning atoms or validating registry membership.
void check_reserved_immediates() {
    constexpr std::array kinds{TermKind::local_pid, TermKind::local_port, TermKind::atom};
    constexpr std::array<Word, 3> tags{0x3, 0x7, 0xb};
    for (std::size_t index = 0; index < tags.size(); ++index) {
        for (const Word payload : {Word{0}, Word{1} << 6, ~Word{0x3f}}) {
            const auto word = payload | tags[index];
            require(classify_immediate(word) == kinds[index], "reserved immediate tag mismatch");
            require(decode_integer(word) == std::unexpected(TermError::wrong_type), "identity decoded as integer");
        }
    }
    require(classify_immediate(0x2b) == TermKind::empty_tuple, "empty tuple misclassified");
    require(classify_immediate(0x3b) == TermKind::empty_list, "nil misclassified");
}

// Reject headers, internal catches and every nonzero payload bit in the canonical empty values.
void check_malformed() {
    constexpr auto invalid = std::unexpected(TermError::invalid_encoding);
    for (Word tag = 0; tag < 64; tag += 4) {
        require(classify_immediate(tag) == invalid, "header classified as an immediate");
        require(decode_integer(tag) == invalid, "header decoded as integer");
    }
    for (const Word word : {Word{0x1b}, Word{0x5b}, ~Word{0x3f} | Word{0x1b}}) {
        require(classify_immediate(word) == invalid, "catch classified as a source value");
        require(decode_integer(word) == invalid, "catch decoded as integer");
    }
    for (unsigned bit = 6; bit < sizeof(Word) * 8; ++bit) {
        require(classify_immediate((Word{1} << bit) | 0x2b) == invalid, "tuple payload accepted");
        require(decode_integer((Word{1} << bit) | 0x3b) == invalid, "nil payload accepted");
    }
}

// Never dereference pointer-shaped words, even null or deliberately unaligned heap reservations.
void check_heap_tags() {
    constexpr auto wrong_type = std::unexpected(TermError::wrong_type);
    for (Word tag = 1; tag < 3; ++tag) {
        for (const Word payload : {Word{0}, Word{4}, ~Word{3}}) {
            require(classify_immediate(payload | tag) == wrong_type, "heap word accepted as immediate");
            require(decode_integer(payload | tag) == wrong_type, "heap word decoded as integer");
        }
    }
    require(decode_integer(0x2b) == wrong_type, "empty tuple decoded as integer");
    require(decode_integer(0x3b) == wrong_type, "nil decoded as integer");
}
} // namespace

// Exercise the standalone runtime without constructing contexts, roots, atom tables or LLVM state.
int main() {
    try {
        check_integers();
        check_reserved_immediates();
        check_malformed();
        check_heap_tags();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
