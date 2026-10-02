# Pattern matching step 12 validation

Windows x64, 2026-10-02. Tuples, cons spines and strings now construct, compare and
match through the shared checked runtime services in function heads and body matches.
The unchanged maint-29 pin is `21776803ecd11f5fa948732c0ec66b8f325dedfc`, checked at
task start. Installed oracle: OTP 29.1.1 / ERTS 17.1. Compiler SDK: LLVM/Clang
23.1.2; quality: Lizard 1.24.0 and clang-tidy 22.1.8.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug configure/build, compiler/runtime/testing enabled | Pass |
| Full CTest suite | 125/125, zero skips, 115.61 s |
| Lizard and full clang-tidy | Pass, all 221 production units |
| Native OTP comparison | 4,801 calls in O0/O2, specialization on/off; positional/project modes |
| Foreign objects | Seven 32/64-bit ELF/Mach-O/COFF targets; no foreign execution |
| Formatting/whitespace | clang-format and git diff --check pass |

The corpus retains 25 complete unchanged helpers from match_SUITE, bif_SUITE and
beam_type_SUITE, including string-prefix alias clauses, tuple/list/nested aliases,
body extraction, first/2, id/1 and literal tuple element access. head_tail/1 retains
its hd/tl assertions; its case-based wrappers are represented by explicitly labeled
ordered-clause guard kernels. The evidence JSON stores source revision/hashes,
function bodies, declarations, adaptation notes and wrapper hashes. Authored cases
cover short/improper lists, exact arities, independent allocation equality, term
order, tuple/list queries, guard constructors, repeated variables, body failures,
source evaluation order and misleading specs. The separate LLVM-free consumer uses
bounded value transport, retains compound results/error payloads across subsequent
calls/growth, checks identity-preserving first/id and requires zero roots on return.

Service-fault consumers in all four policies inspect roots for constructor inputs,
shape/extraction candidates and service outputs. Reached injected OOM/resource/
ownership/unavailable/internal failures stop selection and preserve exact statuses;
retry succeeds with clean channels/frames. A real tiny backing ceiling reached in
a constructing guard produces resource_limit rather than fallback. Nonallocating
retry succeeds. Returned extracted children and constructed badmatch payloads
survive nested calls and 100 later allocations. No moving collection is claimed.

Runtime ownership cases reject forged/interior/foreign starts before dereference,
retain extracted children through stable growth, deny expired graph access and
reject cross-heap graph copying. Allocation ordinal sweeps cover tuple/list backing
and object-index publication, exact rollback, successful retry and balanced teardown.
A compact shared graph exceeds the structural comparison work ceiling; host and
generated comparisons report resource_limit, never inequality. Existing reservation,
root, module, atom and lifecycle fault tests remain enabled.

Earlier semantic/capability fixtures now accept implemented tuples/lists/strings;
batch publication/retry rejection uses deferred comprehensions or control flow.
An initial Lizard finding in the unconditional-head shortcut was resolved by
extracting the constraint predicate, without threshold changes. Stale artifacts
from earlier expectation failures are cleared by test setup. Final full checks
passed on the recorded source. A separate probe accidentally attempted identical
configuration while Ninja held its lock; after quality completed, fresh configuration,
build and all 125 tests passed again. The final copyright-notice-only wrapper edit
was followed by another successful four-policy container corpus run. Local logs: `build/patternmatch-step12/`.

Other native runners and a new sanitizer run were unavailable/not attempted.
No graph copying, collection, scheduling or production executable linking is added.
See [container contract](container-matching.md), [root scopes](generated-roots.md),
and [process memory](runtime-memory.md).
