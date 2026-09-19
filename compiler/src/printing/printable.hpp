#pragma once
#include <erlang_aot/compiler/lexer.hpp>
#include <optional>

namespace erlang_aot::printing {
// Decode integers using Erlang's printable Unicode ranges and escaped control characters.
std::optional<char32_t> printable_character(const Integer &value);
} // namespace erlang_aot::printing
