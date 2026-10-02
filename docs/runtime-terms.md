Current host Terms and factories support owned atoms, arbitrary integers, finite
floats, tuples, lists, maps and exact-length bitstrings. Context admission proves
ownership before any heap access; host handles retain backing and deny access
after context expiration. See [generated roots](generated-roots.md) and the
[numeric](integer-matching.md), [float](float-matching.md), [map](map-matching.md)
and [bitstring](bitstring-matching.md) contracts. Identity/callable/native-record
families and cross-process graph copying remain deferred.

# Runtime immediate-term boundary

Compilation step 10 implements allocation-free word services in
[erlang_aot/runtime/terms.hpp](../runtime/include/erlang_aot/runtime/terms.hpp).
Link `ErlangAoT::generated_program` (or the runtime library for internal consumers).
These functions have no compiler, LLVM, process-context or atom-storage dependency,
never dereference pointer-shaped inputs, and return `TermResult<T>` without throwing.

| Operation | Result | Failure |
| --- | --- | --- |
| `classify_immediate(Word)` | Structural `TermKind` for a small integer, atom, local pid/port, empty tuple or nil | `invalid_encoding` for headers, internal catches or noncanonical empty values; `wrong_type` for list/boxed tags |
| `encode_integer(int64_t)` | Native-width ABI integer word | `out_of_range` if a bignum would be required |
| `decode_integer(Word)` | Signed `int64_t` small integer | `invalid_encoding` for malformed/internal immediates; `wrong_type` for other categories |

Classification checks representation only. Atom/pid/port tags reserve identities;
recognizing their tags does not validate an ID, intern a name, create an identity,
or produce a rooted host `Term`. Empty tuples and nil must be exactly `0x2b` and
`0x3b`; high payload bits are invalid. `TermTag::get_kind()` remains the unchecked
low-bit decoder for private layouts, where header and catch categories are useful.

Integer services delegate to the shared [ABI codec](../abi/include/erlang_aot/abi/term.hpp).
The low nibble is `0xf`, leaving signed 28-bit or 60-bit payloads on 32-bit or 64-bit
runtimes. Bounds are inclusive `[-2^(word_bits-5), 2^(word_bits-5)-1]`; negatives use
unsigned encoding and explicit signed reconstruction. Encoding does not wrap or
silently allocate a bignum. Cross-target compiler code must use the target-width
ABI codec, never these native-runtime services.

```cpp
#include <erlang_aot/runtime/terms.hpp>

// Check the expected result before passing its word to a generated-function argument array.
auto encoded = erlang_aot::runtime::encode_integer(-42);
if (encoded) {
    auto decoded = erlang_aot::runtime::decode_integer(*encoded);
    // decoded contains -42.
}
```

Host `Term` supports checked integer/empty-container values plus owned atoms.
`Term::from_word(word)` admits owner-independent immediates; its context overload
also checks atom membership in that runtime. Atom/boolean accessors and
`TermFactory::atom/boolean` are implemented; heap constructors use the rooted contracts linked above. Host Terms pin immutable spelling records while ABI
words and private heap slots remain one word. See [runtime atoms](runtime-atoms.md)
and [generic dispatch](runtime-builtins.md) for ownership and failure contracts.
Heap prefixes remain private in
[runtime/src/terms/term_layout.hpp](../runtime/src/terms/term_layout.hpp).

These checks report ordinary input errors and remain silent; they are not reached
feature placeholders. Deferred TermFactory service attempts use the shared catalog and
reporting contract separately from these raw validators.

`runtime_generated_link` round-trips signed native boundaries through real dispatch.
`runtime_immediate` exercises overflow, reserved immediate tags,
malformed encodings and hostile pointer-shaped words. `runtime_term_tag` covers all
64 low-tag combinations; the build-only `runtime_term_layout_tests` object target
compiles private prefix assertions without registering a runnable smoke test.
`codegen_runtime_terms` independently builds native LLVM integer constants and checks
runtime classification/encoding/decoding against their bit patterns. Runtime-only
builds run the non-LLVM tests and the standalone generated-program link consumer.
This proves word agreement; execution of LLVM-generated Erlang functions remains
step 40, and native foreign-platform runtime validation remains pending.

Step 12 adds allocation-free `Term::copy_to` / `ProcessHeap::add` for the same
checked immediates and, after pattern/guard step 3, same-runtime atoms; see [process memory ownership](runtime-memory.md). Heap-valued Terms and roots were added in pattern/guard steps 11–16; graph copies remain unavailable.
