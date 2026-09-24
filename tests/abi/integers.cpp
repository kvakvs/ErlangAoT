#include <array>
#include <erlang_aot/abi/term.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace erlang_aot::abi::v1;
static_assert(sizeof(NativeIntegerEncoding::Word) == sizeof(TermWord));
static_assert(alignof(NativeIntegerEncoding::Word) == alignof(TermWord));
static_assert(IntegerEncoding<32>::minimum == -134217728);
static_assert(IntegerEncoding<64>::maximum == 576460752303423487);
static_assert(*IntegerEncoding<32>::encode(-1) == 0xffffffffU);
static_assert(*IntegerEncoding<64>::encode(42) == 0x2afU);

// Keep failures observable in release builds and report a focused contract violation.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Exercise signed boundaries, exact known bit patterns and a dense neighborhood around zero.
template <unsigned Bits> void check_round_trips() {
    using Encoding = IntegerEncoding<Bits>;
    constexpr std::array values{Encoding::minimum, Encoding::minimum + 1, std::int64_t{-42},
                                std::int64_t{-1},  std::int64_t{0},       std::int64_t{1},
                                std::int64_t{42},  Encoding::maximum - 1, Encoding::maximum};
    for (const auto value : values) {
        const auto encoded = Encoding::encode(value);
        require(encoded.has_value(), "boundary integer rejected");
        require(Encoding::decode(*encoded) == value, "boundary integer round trip failed");
    }
    for (std::int64_t value = -100000; value <= 100000; ++value) {
        require(Encoding::decode(*Encoding::encode(value)) == value, "signed integer round trip failed");
    }
    require(*Encoding::encode(Encoding::minimum) == (typename Encoding::Word{1} << (Bits - 1) | 0xfU),
            "minimum encoding changed");
    require(*Encoding::encode(Encoding::maximum) == (~typename Encoding::Word{0} >> 1), "maximum encoding changed");
}

// Values just beyond each target range and every non-integer low tag must fail explicitly.
template <unsigned Bits> void check_rejections() {
    using Encoding = IntegerEncoding<Bits>;
    constexpr std::array overflow{Encoding::minimum - 1, Encoding::maximum + 1,
                                  std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()};
    for (const auto value : overflow) {
        require(Encoding::encode(value) == std::unexpected(IntegerError::out_of_range), "overflow accepted");
    }
    for (typename Encoding::Word tag = 0; tag < 15; ++tag) {
        require(Encoding::decode(tag) == std::unexpected(IntegerError::wrong_tag), "non-integer tag accepted");
    }
}

// Test both target widths on every host without requiring the compiler or runtime library.
int main() {
    try {
        check_round_trips<32>();
        check_round_trips<64>();
        check_rejections<32>();
        check_rejections<64>();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
