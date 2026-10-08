#pragma once
#include "v1.hpp"
#include <cstddef>

namespace clause::abi::v1 {
// Every frame starts with these header words: the caller's header offset, the function's FrameDescriptor, the
// resume index its body switches on and the active handler index (0 while exceptions return through callers).
inline constexpr std::size_t frame_header_words = 4;
inline constexpr std::size_t frame_resume_word = 2;
// Argument and result registers (BEAM X registers): one per possible argument, results in the first.
inline constexpr std::size_t register_count = 256;
} // namespace clause::abi::v1

// Generated code moves between functions only by tail transfers to the Code these services return.
// Push a zeroed frame for `frame` (a FrameDescriptor), copy its arguments from the registers into its first slots
// and return its body. When the stack budget is exhausted, record the failure and return the caller's body instead.
// Entry is a safepoint: it first collects when the heap asks for it, with the arguments as register roots.
void *CLAUSE_enter_v1(void *context, const void *frame) noexcept;
// Tail call: release the current frame, then enter `frame` as CLAUSE_enter_v1 does.
void *CLAUSE_tail_v1(void *context, const void *frame) noexcept;
// Return `result` in the first register, release the current frame and return the body of the frame below.
void *CLAUSE_return_v1(void *context, clause::abi::v1::TermWord result) noexcept;
// Loop-head safepoint: collect when the heap asks for it. Term slots may be rewritten; the stack never moves.
void CLAUSE_safepoint_v1(void *context) noexcept;
// Header of the current frame, read once when a body is entered; its slots follow the header.
clause::abi::v1::TermWord *CLAUSE_frame_v1(void *context) noexcept;
// The process registers; their address is stable for the context's lifetime.
clause::abi::v1::TermWord *CLAUSE_registers_v1(void *context) noexcept;
// Host entry: run the function of `frame` with `arguments` to completion above a bottom frame and return its
// result, or 0 with a failure in the checked channel.
clause::abi::v1::TermWord CLAUSE_invoke_v1(void *context, const void *frame,
                                           const clause::abi::v1::TermWord *arguments) noexcept;
