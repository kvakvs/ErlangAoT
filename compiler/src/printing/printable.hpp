#pragma once
#include <clause/compiler/lexer.hpp>
#include <optional>

namespace clause::printing {
// Decode integers using Erlang's printable Unicode ranges and escaped control characters.
std::optional<char32_t> printable_character(const Integer &value);
} // namespace clause::printing
