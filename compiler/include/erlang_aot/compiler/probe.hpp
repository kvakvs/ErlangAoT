#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace erlang_aot {
struct DirectiveProbe {
    // Own the parsed name independently of the input buffer.
    std::string name;
    // Record the byte range of the name for diagnostics.
    std::size_t begin;
    std::size_t end;
};

// Demonstrate recursive character parsing without committing preprocessing
// state.
std::optional<DirectiveProbe> probe_directive(std::string_view source);
} // namespace erlang_aot
