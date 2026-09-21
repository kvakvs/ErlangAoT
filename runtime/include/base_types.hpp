#pragma once
#include <cstdint>

namespace erlang_aot::runtime {

// Use the runtime target's pointer width, never the compiler host's width for cross emission.
using Word = std::uintptr_t;
static_assert(sizeof(Word) == 4 || sizeof(Word) == 8);
static_assert(alignof(Word) == sizeof(Word));

static constexpr Word ERL_WORD_BITS = 8 * sizeof(Word);

} // namespace erlang_aot::runtime