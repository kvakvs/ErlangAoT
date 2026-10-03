#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace erlang_aot::project {
struct EntryName {
    // Decoded atom text of the entry module and function; the entry arity is always 1 (argv list).
    std::u32string module;
    std::u32string function;
};

struct SelectedEntry {
    // Requested entry and where it came from ("--entry" or a manifest site) for unlocated diagnostics.
    EntryName name;
    std::string origin;
};

// Decode MODULE or MODULE:FUNCTION (function defaults to main); reject malformed atom text.
std::optional<EntryName> parse_entry(std::string_view text);
// Render an entry as MODULE:FUNCTION/1 for diagnostics.
std::string entry_text(const EntryName &entry);
} // namespace erlang_aot::project
