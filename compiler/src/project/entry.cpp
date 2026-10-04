#include "entry.hpp"
#include "diagnostics.hpp"
#include "glob_utf8.hpp"
#include <algorithm>
#include <erlang_aot/compiler/source.hpp>

namespace erlang_aot::project {
namespace {
// Decode strict UTF-8, treating any encoding failure as an invalid spelling.
std::optional<std::u32string> decode(const std::string_view text) {
    try {
        return filename_scalars(text, {});
    } catch (const Failure &) {
        return std::nullopt;
    }
}

// Accept 1..255 scalars without separators or control characters, matching the atom length limit.
bool valid_name(const std::optional<std::u32string> &name) {
    return name && !name->empty() && name->size() <= 255 &&
           std::ranges::none_of(*name, [](const char32_t value) { return value < 0x20 || value == 0x7f || value == U':'; });
}
} // namespace

std::optional<EntryName> parse_entry(std::string_view text) {
    const auto colon = text.find(':');
    const auto module = decode(text.substr(0, colon));
    const auto function =
        colon == std::string_view::npos ? std::optional<std::u32string>(U"main") : decode(text.substr(colon + 1));
    if (!valid_name(module) || !valid_name(function)) {
        return std::nullopt;
    }
    return EntryName{*module, *function};
}

std::string entry_text(const EntryName &entry) { return utf8(entry.module) + ":" + utf8(entry.function) + "/1"; }
} // namespace erlang_aot::project
