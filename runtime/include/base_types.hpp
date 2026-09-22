#pragma once
#include <cstdint>

namespace erlang_aot::runtime {

// Use the runtime target's pointer width, never the compiler host's width for cross emission.
using Word = std::uintptr_t;
static_assert(sizeof(Word) == 4 || sizeof(Word) == 8);
static_assert(alignof(Word) == sizeof(Word));

static constexpr Word ERL_WORD_BITS = 8 * sizeof(Word);

enum class TermTagPrimary : std::uint8_t {
    header = 0,
    list = 1,  // the rest of the bits point to a cons cell
    boxed = 2, // the rest of the bits point to a Header object, a boxed in memory
    immed = 3, // if tag1 == immed, allows reading tag2
};

enum class TermTag2 : std::uint8_t {
    pid = 0,      // if tag1 == immed
    port = 1,     // if tag1 == immed
    immed2 = 2,   // if tag1 == immed, and tag2 == immed2, allows reading into tag3
    smallint = 3, // if tag1 == immed
};

enum class TermTag3 : std::uint8_t {
    atom = 0,
    catch_object = 1,
    nil = 3,
};
} // namespace erlang_aot::runtime