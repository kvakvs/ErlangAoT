# Generated-code ABI (revision 4)

Private contract between compiler output and the runtime. It is project-internal
C++23, not BEAM-compatible and not a general FFI. Objects and runtime must come
from the same build; older descriptor revisions are rejected before use.

Headers: [v1.hpp](../abi/include/erlang_aot/abi/v1.hpp) (term/context/function
types), [term.hpp](../abi/include/erlang_aot/abi/term.hpp) (immediate integer
codec), [status.hpp](../abi/include/erlang_aot/abi/status.hpp),
[builtins.hpp](../abi/include/erlang_aot/abi/builtins.hpp),
[startup.hpp](../abi/include/erlang_aot/abi/startup.hpp) (program startup).

## Terms

A term is one unsigned target-pointer-width word (32 or 64 bits). Layouts derive
from the configured LLVM target, so cross-target code uses target widths.

- Small integers: low four bits `0xf`; signed payload of `word_bits - 4` bits,
  range `[-2^(word_bits-5), 2^(word_bits-5)-1]`. Larger values are heap bignums.
- Atoms: low six bits `0x0b`, non-recycled process-wide payload. IDs are never
  serialized or used for ordering.
- Empty tuple `0x2b` and nil `0x3b` exactly; other payload bits are invalid.
- Boxed and list words point into the owning process heap and are admitted only
  after the runtime proves ownership (see [terms](terms.md)).

## Functions and symbols

Entry signature (native C calling convention, no C linkage required):

```cpp
TermWord function(ProcessContext *context, const TermWord *arguments);
```

Arguments are a borrowed, word-aligned array in source order (null at arity 0).
The context is live and passed unchanged through calls. A returned word is
usable only after checking the failure channel.

Symbols: `eav1_<hex module>_<hex function>_<arity>`, lowercase hex of UTF-8
bytes, canonical decimal arity; reversible and host-independent. Exported entries
are external, others internal.

## Module registration

Each module emits `eav1_<hex module>__0.descriptor` and `.register`. The
descriptor holds ABI version, term width, export table and atom spellings
(UTF-8 pointer/size pairs). Registration calls
`erlang_aot_register_module_v4(Runtime*)`, which validates version/width and all
exports, interns atoms, builds a frozen registry and publishes it with the code
image in one transaction. Duplicate modules never replace code; any failure
publishes nothing (already interned atoms stay in the bounded table).

- Call `.register` explicitly before resolving exports. There are no global
  constructors; static-archive users must reference registration entries.
- `llvm.used` keeps descriptors and entries; linking without the runtime fails
  on the missing service symbol.
- The descriptor address is the atom-binding key; its image must stay mapped for
  the module lifetime. Each runtime has its own bindings for the same image.
- Atom expressions read slots via `erlang_aot_atom_v3`; they never intern.
- A startup object (`eav1_start`) lists every descriptor in a
  `StartupDescriptor` and its native `main` calls
  `erlang_aot_main_v1(argc, argv, descriptor)`, which registers all modules
  and runs the entry ([executables](executables.md#startup-object)).

## Failure channel (revision 2)

Errors are not encoded in term bits. After every non-tail generated call the
caller checks `erlang_aot_call_failed_v2(context)` before using the result or
evaluating the next argument. On failure the callee returns an invalid zero word.

| Outcome | Transport |
| --- | --- |
| Pattern mismatch, guard rejection | Continuation to next candidate; channel untouched |
| Exhausted clauses | `error:function_clause`; `error:{case_clause, Value}` with owned payload for a `case`; `error:if_clause` |
| Body match failure | `error:{badmatch, Value}` with owned payload |
| Record access, bad arguments, arithmetic, maps | `badrecord`, `badarg`, `badarith`, `badmap`/`badkey` |
| Invalid lazy left operand | `{badarg, Value}` |
| Infrastructure (OOM, limits, ownership, internal) | `CallError::runtime_failure` with exact `Status` |
| `erlang:error/1,2,3`, `exit/1`, `throw/1` | `raised_error`/`raised_exit`/`raised_throw`: class from the ID, owned payload is the whole reason |
| `erlang:halt/0,1` | `CallError::halted` with `halt_status` (and slogan) |

Reasons are typed IDs recorded by `erlang_aot_raise_v2`; the three `raised_*`
IDs select class `exit` or `throw` (otherwise `error`) and carry any term as the
reason. `error/2,3` evaluate their extra arguments and drop them until stack
traces exist (step 15). First failure wins; nested invocations share
the channel. `GeneratedInvocation` is the host scope: it checks pending failures
before entry and after return, copies result or error, and clears only at the
outermost exit (also on C++ exceptions). No exception crosses generated entries.
Raw entry callers must open a `GeneratedInvocation`; normal hosts use
`ResolvedFunction::call`.

## Root scopes

Every generated function calls `erlang_aot_roots_enter_v4(context, count)` before
loading arguments and `erlang_aot_roots_leave_v4(context, frame, result)` on
return. A frame is a zeroed window of target-word slots on the process root
stack (stable segments apart from the heap), bounded per context to 1,000,000
live words and 4,096 frames.

- Arguments occupy persistent slots; every evaluated value is stored in a slot
  before the next expression or call. Failed candidates clear their slots.
- Results become root words in the parent's handoff (BEAM X registers) before
  the frame is released; error payloads are root words of the channel (BEAM
  `fvalue`). LIFO violations are infrastructure errors.
- Heap allocation is the future GC safepoint: all live values are rooted there.
  Generated code never collects yet; only a host `collect()` outside generated
  calls moves the heap.

## Runtime services

Generated code calls checked C++ services: `erlang_aot_exact_v1` (exact
equality), `erlang_aot_immediate_v1` (immediate predicates/queries),
`erlang_aot_construct_v1`, `erlang_aot_inspect_v1`, `erlang_aot_integer_v1`,
`erlang_aot_float_v1`, `erlang_aot_map_v1`, `erlang_aot_bits_v1`. Each returns
success, semantic error (`badarg`/`badarith`/...) or infrastructure failure and
writes output only on success. `erlang_aot_display_v1` ([output.hpp](../abi/include/erlang_aot/abi/output.hpp))
prints one `erlang:display/1` line and yields `true`; it has no semantic error.
`erlang_aot_halt_v1` never succeeds: it records a halt request
(`CallError::halted` with the exit status) or `badarg`, so the caller unwinds.
Linker spellings follow the target's Itanium or Microsoft C++ mangling.

`abi::v1::dispatch_builtin` calls host-registered builtins by module/function
bytes, argument array and arity, returning a `Status`; output is written only on
success.

| Status | Value | Meaning |
| --- | ---: | --- |
| `ok` | 0 | Success |
| `not_implemented` | 1 | Known deferred feature, reported once |
| `invalid_argument` | 2 | Invalid input |
| `diagnostic_failure` | 3 | Report delivery failed |
| `out_of_memory` | 4 | Allocation failed, state rolled back |
| `busy` | 5 | Live contexts or running dispatch |
| `wrong_owner` | 6 | Object belongs to another runtime/context |
| `resource_limit` | 7 | Cap or budget exhausted |
| `stopped` | 8 | Owner shut down |
| `abi_mismatch` | 9 | Version or term width differs |
| `internal_error` | 10 | Unexpected failure contained |
| `unknown_builtin` | 11 | Signature not registered or catalogued |
| `erlang_error` | 12 | Structured Erlang error recorded |
| `output_failure` | 13 | Standard output rejected a write |

## Revisions

| Revision | Change |
| --- | --- |
| 1 | Initial descriptors and entries |
| 2 | Checked failure channel |
| 3 | Atom spellings and slots in descriptors |
| 4 | Mandatory generated root scopes |
