// Step-17 decision prototype: Erlang functions lowered by hand the way the compiler would emit them. Each
// framed function has an entry (push frame) and a body that switches on the frame's continuation index.
#include "model.hpp"

namespace prototype {
namespace {
Result sum_body(Process *process);
Result fail_body(Process *process);
Result catcher_body(Process *process);

const Descriptor SUM{sum, sum_body, "sum", 1, 1};
const Descriptor FAIL{fail, fail_body, "fail", 1, 1};
const Descriptor CATCHER{catcher, catcher_body, "catcher", 1, 1};

// sum(0) -> 0; sum(N) -> N + sum(N - 1).   Body recursion: one frame per level.
Result sum_body(Process *process) {
    Word *roots = slots(process);
    switch (frame(process)->resume) {
    case 0:
        if (roots[0] == 0) {
            probe(process);
            process->x[0] = 0;
            TRANSFER(leave(process));
        }
        frame(process)->resume = 1;
        process->x[0] = roots[0] - 1;
        TRANSFER(sum);
    default:
        // Resumed after sum(N - 1): its result is in x0 and N is reloaded from the (possibly moved) frame.
        process->x[0] += roots[0];
        TRANSFER(leave(process));
    }
}

// fail(0) -> error(boom); fail(N) -> 1 + fail(N - 1).
Result fail_body(Process *process) {
    Word *roots = slots(process);
    switch (frame(process)->resume) {
    case 0:
        if (roots[0] == 0) {
            TRANSFER(raise(process, BOOM));
        }
        frame(process)->resume = 1;
        process->x[0] = roots[0] - 1;
        TRANSFER(fail);
    default:
        process->x[0] += 1;
        TRANSFER(leave(process));
    }
}

// catcher(N) -> try fail(N) catch error:R -> R end.   Continuation 1 is the normal return, 2 the handler.
Result catcher_body(Process *process) {
    Word *roots = slots(process);
    switch (frame(process)->resume) {
    case 0:
        frame(process)->handler = 2;
        frame(process)->resume = 1;
        process->x[0] = roots[0];
        TRANSFER(fail);
    case 1:
        frame(process)->handler = 0;
        TRANSFER(leave(process));
    default:
        frame(process)->handler = 0;
        process->x[0] = exception(process);
        TRANSFER(leave(process));
    }
}
} // namespace

// answer() -> 42.   Frameless: returns straight to the caller's body.
Result answer(Process *process) {
    if (tick(process, answer)) {
        SUSPEND();
    }
    process->x[0] = 42;
    TRANSFER(caller(process));
}

// loop(0, Acc) -> Acc; loop(N, Acc) -> loop(N - 1, Acc + 1).   Frameless tail recursion.
Result loop(Process *process) {
    if (tick(process, loop)) {
        SUSPEND();
    }
    if (process->x[0] == 0) {
        process->x[0] = process->x[1];
        TRANSFER(caller(process));
    }
    process->x[0] -= 1;
    process->x[1] += 1;
    TRANSFER(loop);
}

Result sum(Process *process) { TRANSFER(call(process, &SUM)); }

Result fail(Process *process) { TRANSFER(call(process, &FAIL)); }

Result catcher(Process *process) { TRANSFER(call(process, &CATCHER)); }
} // namespace prototype
