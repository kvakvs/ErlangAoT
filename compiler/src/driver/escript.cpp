#include "escript.hpp"
#include <algorithm>
#include <cstdint>
#include <format>

namespace erlang_aot::cli {
namespace {
// Return zero-based line `number` without its terminator; empty past the end.
std::string_view line(const std::string_view bytes, const std::size_t number) {
    std::size_t begin = 0;
    for (std::size_t index = 0; index < number; ++index) {
        const auto end = bytes.find('\n', begin);
        if (end == std::string_view::npos) {
            return {};
        }
        begin = end + 1;
    }
    return bytes.substr(begin, bytes.find('\n', begin) - begin);
}

// Locate "%%!" as escript does: on line 2, or on line 3 after a comment on line 2.
std::optional<std::size_t> emulator_line(const std::string_view bytes) {
    if (line(bytes, 1).starts_with("%%!")) {
        return 2;
    }
    if (line(bytes, 1).starts_with('%') && line(bytes, 2).starts_with("%%!")) {
        return 3;
    }
    return std::nullopt;
}

// Skip whitespace and % comments starting at `at`.
std::size_t skip_layout(std::string_view text, std::size_t at) {
    while (at < text.size()) {
        if (text[at] == '%') {
            at = std::min(text.find('\n', at), text.size());
        } else if (text[at] != ' ' && text[at] != '\t' && text[at] != '\r' && text[at] != '\n') {
            return at;
        } else {
            ++at;
        }
    }
    return at;
}

// Mirror escript: only a first form spelled "-module(" declares the module explicitly.
bool declares_module(const std::string_view body) {
    auto at = skip_layout(body, 0);
    if (at >= body.size() || body[at] != '-') {
        return false;
    }
    at = skip_layout(body, at + 1);
    if (!body.substr(at).starts_with("module")) {
        return false;
    }
    at = skip_layout(body, at + 6);
    return at < body.size() && body[at] == '(';
}

// Quote atom text, escaping anything outside printable ASCII as \x{H} so source encoding never matters.
std::string atom_literal(const std::u32string_view name) {
    std::string result = "'";
    for (const auto value : name) {
        if (value >= 0x20 && value < 0x7f && value != U'\'' && value != U'\\') {
            result.push_back(static_cast<char>(value));
        } else {
            result += std::format("\\x{{{:X}}}", static_cast<std::uint32_t>(value));
        }
    }
    return result + "'";
}
} // namespace

std::u32string escript_module_name(const std::filesystem::path &path) {
    auto name = path.filename().u32string();
    std::ranges::replace(name, U'.', U'_');
    return name + U"__escript";
}

std::optional<EscriptSource> escript_source(const std::filesystem::path &path, std::string_view bytes) {
    if (!bytes.starts_with("#!")) {
        return std::nullopt;
    }
    auto end = std::min(bytes.find('\n'), bytes.size());
    if (end > 0 && bytes[end - 1] == '\r') {
        --end;
    }
    const auto rest = bytes.substr(end);
    const auto header =
        declares_module(rest) ? std::string{} : "-module(" + atom_literal(escript_module_name(path)) + ").";
    return EscriptSource{header + std::string(rest), emulator_line(bytes)};
}
} // namespace erlang_aot::cli
