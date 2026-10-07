#include "startup.hpp"
#include "terms.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <corecrt_startup.h>
#include <stdlib.h>
#endif

namespace erlang_aot::runtime::detail {
namespace {
// Unicode code points of one argument, in order.
using Points = std::vector<std::uint32_t>;

// True for UTF-16 surrogate code points, which are never Unicode scalar values.
bool surrogate(std::uint32_t point) { return point >= 0xD800 && point <= 0xDFFF; }

#ifdef _WIN32
// Combine the surrogate pair starting at `at`, or return 0 when the units do not form one.
std::uint32_t pair(std::wstring_view text, std::size_t at) {
    if (at + 1 >= text.size()) {
        return 0;
    }
    const std::uint32_t high = text[at];
    const std::uint32_t low = text[at + 1];
    const bool valid = high >= 0xD800 && high <= 0xDBFF && low >= 0xDC00 && low <= 0xDFFF;
    return valid ? 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00) : 0;
}

// Decode UTF-16 code units; an unpaired surrogate becomes U+FFFD.
Points decode(std::wstring_view text) {
    Points points;
    points.reserve(text.size());
    for (std::size_t at = 0; at < text.size(); ++at) {
        const auto combined = pair(text, at);
        const std::uint32_t unit = text[at];
        points.push_back(combined != 0 ? combined : (surrogate(unit) ? 0xFFFD : unit));
        at += combined != 0 ? 1 : 0;
    }
    return points;
}

// Ask the CRT for its wide argument vector; the narrow argv of `main` would lose non-ANSI characters.
std::vector<Points> native_arguments(int, char **) {
    if (_configure_wide_argv(_crt_argv_unexpanded_arguments) != 0 || !__wargv) {
        throw std::runtime_error("wide command line unavailable");
    }
    std::vector<Points> arguments;
    arguments.reserve(__argc > 1 ? static_cast<std::size_t>(__argc - 1) : 0);
    for (auto **argument = __wargv; *argument; ++argument) {
        if (argument != __wargv) {
            arguments.push_back(decode(*argument));
        }
    }
    return arguments;
}
#else
// Sequence length announced by a UTF-8 lead byte; 0 for ASCII, continuation and never-valid leads.
std::size_t lead_size(unsigned char lead) {
    if (lead < 0xC2 || lead > 0xF4) {
        return 0;
    }
    return lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2);
}

// Length of a valid UTF-8 sequence starting at `at` (shortest form, no surrogates), or 0.
std::size_t sequence(std::string_view bytes, std::size_t at, std::uint32_t &point) {
    const auto lead = static_cast<unsigned char>(bytes[at]);
    const auto size = lead_size(lead);
    if (size == 0 || at + size > bytes.size()) {
        return 0;
    }
    point = lead & (0x7FU >> size);
    for (std::size_t i = 1; i < size; ++i) {
        const auto byte = static_cast<unsigned char>(bytes[at + i]);
        if ((byte & 0xC0U) != 0x80U) {
            return 0;
        }
        point = (point << 6) | (byte & 0x3FU);
    }
    constexpr std::array<std::uint32_t, 5> minimum{0, 0, 0x80, 0x800, 0x10000};
    return point >= minimum[size] && point <= 0x10FFFF && !surrogate(point) ? size : 0;
}

// Decode UTF-8; a byte that does not start a valid sequence becomes its own (Latin-1) code point.
Points decode(std::string_view bytes) {
    Points points;
    points.reserve(bytes.size());
    for (std::size_t at = 0; at < bytes.size();) {
        std::uint32_t point = 0;
        const auto size = static_cast<unsigned char>(bytes[at]) < 0x80 ? 0 : sequence(bytes, at, point);
        points.push_back(size == 0 ? static_cast<unsigned char>(bytes[at]) : point);
        at += size == 0 ? 1 : size;
    }
    return points;
}

// POSIX arguments are bytes; skip the program name.
std::vector<Points> native_arguments(int argc, char **argv) {
    std::vector<Points> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc && argv; ++i) {
        arguments.push_back(decode(argv[i] ? argv[i] : ""));
    }
    return arguments;
}
#endif

// Build one string term from its code points.
TermResult<Term> string_term(TermFactory &factory, const Points &points) {
    std::vector<Term> characters;
    characters.reserve(points.size());
    for (const auto point : points) {
        const auto character = factory.integer(point);
        if (!character) {
            return character;
        }
        characters.push_back(*character);
    }
    return factory.list(characters);
}
} // namespace

TermResult<Term> program_arguments(ProcessContext &context, int argc, char **argv, std::size_t skip) {
    TermFactory factory(context);
    const auto arguments = native_arguments(argc, argv);
    const auto remaining = std::span(arguments).subspan(std::min(skip, arguments.size()));
    std::vector<Term> strings;
    strings.reserve(remaining.size());
    for (const auto &points : remaining) {
        const auto string = string_term(factory, points);
        if (!string) {
            return string;
        }
        strings.push_back(*string);
    }
    return factory.list(strings);
}
} // namespace erlang_aot::runtime::detail
