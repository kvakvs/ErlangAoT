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

Step 11 adds a minimal immediate-only `Term` in the namespaced header for
[generic builtin dispatch](runtime-builtins.md). `Term::from_word` accepts small
integers and canonical empty containers; identities and heap values are rejected.
`word()`, `kind()` and `integer_value()` are implemented. Copy/move are word copies
without roots. Other semantic/heap operations remain reserved until ownership
machinery exists. Step 14 adds [TermFactory](../runtime/include/terms.hpp) lifetime
bindings and explicit reporting placeholders for all constructors; they create no
terms or roots. See [runtime services](runtime-services.md). Heap prefixes live privately in
[runtime/src/terms/term_layout.hpp](../runtime/src/terms/term_layout.hpp).
Future atom construction stays with runtime-wide
[AtomStorage](../runtime/include/atom_storage.hpp), including loaded-code roots;
no separate atom table is introduced.

These checks report ordinary input errors and remain silent; they are not reached
feature placeholders. TermFactory service attempts use the shared catalog and
reporting contract separately from these raw validators.

`runtime_immediate` exercises signed boundaries, overflow, reserved immediate tags,
malformed encodings and hostile pointer-shaped words. `runtime_term_tag` covers all
64 low-tag combinations; `runtime_term_layout` compiles private prefix assertions.
`codegen_runtime_terms` independently builds native LLVM integer constants and checks
runtime classification/encoding/decoding against their bit patterns. Runtime-only
builds run the non-LLVM tests and the standalone generated-program link consumer.
This proves word agreement; execution of LLVM-generated Erlang functions remains
step 40, and native foreign-platform runtime validation remains pending.

Step 12 adds allocation-free `Term::copy_to` / `ProcessHeap::add` for the same
checked immediates; see [process memory ownership](runtime-memory.md). Heap-valued
Terms, graph copies and roots remain unavailable.
