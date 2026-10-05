// Step-17 decision prototype (docs/execution-model.md): process, frame and transfer layout shared by the
// hand-lowered "generated" functions and the prototype runtime. Freestanding so it compiles for every target.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace prototype {
using Word = uintptr_t;
struct Process;

#if defined(PROTOTYPE_TRAMPOLINE)
struct Step;
using Code = Step (*)(Process *);

// Trampoline fallback: a transfer returns the next code to the scheduler loop; null suspends the process.
struct Step {
    Code next;
};

using Result = Step;
#define TRANSFER(code)                                                                                                 \
    return ::prototype::Step { code }
#define SUSPEND()                                                                                                      \
    return ::prototype::Step { nullptr }
#else
using Code = void (*)(Process *);
using Result = void;
// Guaranteed tail call: every transfer is a jump, so native depth stays one call above the scheduler.
#define TRANSFER(code) [[clang::musttail]] return (code)(process)
#define SUSPEND() return
#endif

struct Descriptor {
    // Fresh-call entry (a yield resumes here) and the body that resumes this function's frames.
    Code entry;
    Code body;
    // Stack-trace name and the fixed number of term slots after the frame header.
    const char *name;
    size_t slots;
    size_t arity;
};

struct Frame {
    // Offset of the caller's header: frames are linked by offsets, so the stack may move.
    size_t previous;
    // Names the body to resume, the trace entry and the slot count.
    const Descriptor *function;
    // Continuation index the body switches on when control returns to this frame.
    Word resume;
    // Continuation index of the innermost active handler in this frame; 0 when none.
    Word handler;
};

inline constexpr size_t HEADER_WORDS = sizeof(Frame) / sizeof(Word);
inline constexpr size_t REGISTERS = 4;
inline constexpr size_t TRACE_DEPTH = 8;

struct Process {
    // Flat stack: frame headers followed by their slots; `frame` and `top` are word offsets into it.
    Word *stack;
    size_t capacity;
    size_t frame;
    size_t top;
    // Argument and result registers (BEAM X); the first `live` words are roots at a transfer.
    Word x[REGISTERS];
    size_t live;
    // Calls left in this time slice and the entry a suspended process continues at.
    long reductions;
    Code resume_at;
    // Failure channel: a pending error reason and the innermost frames named when it was raised.
    bool failed;
    Word reason;
    const char *trace[TRACE_DEPTH];
    // Exit state read by the scheduler.
    bool exited;
    bool crashed;
    // Measurements: stack moves, peak stack words, suspensions and the lowest native stack address reached.
    size_t moves;
    size_t peak;
    size_t yields;
    uintptr_t deepest;
};

// Runtime services called with ordinary native calls; none of them re-enters generated code.
// Count one reduction and push the callee's zeroed frame holding its arguments (moving the stack when full);
// return the body to transfer to, or the parking code that suspends the process when the slice is used up.
Code call(Process *process, const Descriptor *function);
// Pop the current frame and return the caller's body; its frame header holds the continuation index.
Code leave(Process *process);
// Record an error, name the innermost frames and unwind to the innermost frame with a handler.
Code raise(Process *process, Word reason);
// Take the pending error reason and clear the channel (the handler's `Class:Reason` binding).
Word exception(Process *process);
// Count one reduction for a frameless function; true when it must suspend and resume at `entry`.
bool tick(Process *process, Code entry);
// Code that returns to the scheduler; transferring to it suspends the process.
Result park(Process *process);
// Record how deep the native stack is at this point.
void probe(Process *process);

// The current frame's header and slots, recomputed from offsets after every transfer.
inline Frame *frame(Process *process) { return reinterpret_cast<Frame *>(process->stack + process->frame); }

inline Word *slots(Process *process) { return process->stack + process->frame + HEADER_WORDS; }

// Body of the current frame's function: where a frameless callee returns to.
inline Code caller(Process *process) { return frame(process)->function->body; }

// Prototype atom for `boom`.
inline constexpr Word BOOM = 0xB00;

// Hand-lowered example functions (generated.cpp).
Result answer(Process *process);
Result sum(Process *process);
Result loop(Process *process);
Result fail(Process *process);
Result catcher(Process *process);
} // namespace prototype
