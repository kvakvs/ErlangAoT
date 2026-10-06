# Term representations

Admitted kinds: atoms/booleans, arbitrary integers, finite binary64 floats,
tuples, proper/improper lists and strings, maps, bitstrings and ordinary tuple
records. Pids, ports, references, funs and native records are not representable
yet. Word encodings are in [abi.md](abi.md#terms).

## Ownership

- Compound values live in their process heap ([runtime](runtime.md#process-memory)).
  Process pointers always name object starts. Admission checks ownership: the
  word points, word-aligned, below the top of the process's heap block or one of
  its fragments,
  and the header (or cons cell) there matches its tag
  ([admission](runtime-heap.md#admission)). Foreign and stale words are rejected
  without any load. Kind and extent are decoded from the header.
- A host `Term` for a heap value is a raw tagged word, valid until its heap's
  next collection ([roots](runtime-heap.md#roots-and-safe-points)); it does not
  pin heap storage. After context teardown, access returns `expired_context`;
  after a later collection, `stale_term`. Terms are always safe to destroy.
- Construction validates children, reserves, initializes, then publishes in one
  step; failure rolls back backing and counters.
- `Term::from_word(word)` admits only owner-independent immediates;
  `Term::from_word(word, context)` also admits atoms and heap terms of that
  context. Same-heap handoff keeps identity. `copy_to`/`ProcessHeap::add`
  copy a graph of another process of the same runtime with its sharing; term
  factories refuse foreign inputs with `wrong_owner`
  ([copying between heaps](runtime-heap.md#copying-between-heaps)).
- Cells live until an explicit collection finds them unreachable, or until
  heap teardown.

## Atoms

- Per-runtime `AtomStorage`, lazily interned by exact UTF-8 spelling, no
  normalization or eviction. `RuntimeOptions::max_atoms` 1..2^26, default 2^20;
  module/export names count. Existing spellings succeed at capacity.
- Up to 255 Unicode scalars; empty, NUL and supplementary characters allowed;
  malformed/overlong/surrogate UTF-8 rejected.
- Payloads come from a process-wide counter so a foreign runtime's atom word is
  detectable. 32-bit processes have 2^26 lifetime identities in total.
- Host atom Terms pin the spelling, surviving runtime teardown. Moving an atom
  between runtimes means interning `atom_utf8()` in the destination.
- Booleans are the atoms `true`/`false`.

## Integers

- Values in the target's 28/60-bit payload are immediates; larger ones are
  immutable sign/magnitude cells. Zero and small values always normalize to
  immediates. No host-width narrowing of literals.
- Generated `+`, `-`, `*` try an inline fast path on two immediates (double-width
  compute with explicit bounds), otherwise call the runtime service.
- `div` truncates toward zero; `rem` takes the dividend's sign; bitwise ops use
  infinite two's complement; negative shift counts reverse direction; huge right
  shifts saturate to 0 or -1.
- Errors: wrong operands and zero divisor → `badarith`; `abs/1` → `badarg`.
- Limits: as ERTS, a magnitude of at most `BIG_ARITY_MAX` words: 4,194,240
  bits on 64-bit targets (65,535 words), 4,194,272 on 32-bit (131,071 words);
  decimal text follows (1,262,593 and 1,262,602 digits). A larger arithmetic
  result raises `error:system_limit` in a body (`ValueOutcome::system_limit`,
  [ABI](abi.md#failure-channel-revision-2)) and fails a guard; an integer
  segment extracting a larger value does not match. The compiler rejects a
  literal past 4,194,240 bits (`illegal integer`, as OTP's scanner) and a
  constant pattern past it (`illegal pattern`).

## Floats

- IEEE binary64 bits pass to the runtime as eight network-order bytes; NaN and
  infinity are rejected. No fast-math.
- `+ - *` stay exact on two integers; any float operand uses binary64. `/`
  always converts both. Nonfinite results and zero divisors → `badarith`.
- `float/1` rounds to nearest-even; `round/1` ties away from zero; `trunc`,
  `floor`, `ceil` return arbitrary integers. Bad operands → `badarg`.
- Exact equality distinguishes `1` from `1.0` and `0.0` from `-0.0`; numeric
  comparison compares the float's exact integer part and fraction, never
  rounding the integer. `min/max` return the first operand on ties.

## Tuples, lists, strings

- Tuple: arity header + fields. Cons: head + tail words. `{}` and `[]` are
  immediates. Strings are lists of code points. Lists have no length cap
  beyond memory (an optional heap budget included). Tuples hold up to
  16,777,215 elements (`MAX_TUPLE_ARITY`, OTP's `MAX_ARITYVAL`); constructors
  report a larger one as `resource_limit`, builtins will raise `badarg`.
- Services: `hd`, `tl`, `length`, `tuple_size`, `size`, one-based `element`.

## Maps

- Immutable tables sorted by exact key order. Duplicate construction keys keep
  the last value. Construction sorts the keys (O(n log n) comparisons; already
  ascending keys are only checked); updates insert by binary search. No size
  or work cap beyond memory, as in OTP; on 32-bit targets the header's word
  count bounds a map at 2^24 - 1 entries (`resource_limit`). Integer and float keys differ (also `0.0` vs `-0.0`, also
  nested).
- `K := V` updates require the key; `K => V` inserts or replaces. Updates stage
  a new table and publish once.
- Body errors: `{badmap, M}`, `{badkey, K}`; guards reject instead.
- Services: `is_map`, `map_size`, `map_get`, `is_map_key`, construction, update.

## Bitstrings

- Packed MSB-first with exact bit length, zeroed padding. Up to 64 bytes live
  inline in a heap binary sized to the data; larger values use a shared
  immutable buffer outside the heap, viewed by off-heap binary cells that
  extracted tails share. The buffer is charged once to the creating process.
  There is no size cap beyond an optional process heap budget; integer
  segments are written without building an integer as wide as the segment.
- Construction stages all segments before publishing. Integer segments truncate;
  native endianness comes from the target data layout.
- Float segments: widths 16/32/64; construction may encode infinity, but
  matching rejects infinite/NaN fields. Zero-width float matches extract `0.0`.
- UTF-8/16/32 segments validate scalars, surrogates and truncation.
- Matching advances an explicit bit cursor only on success; `:all` as an explicit
  size is invalid.
- Services: `is_binary`, `is_bitstring`, `bit_size`, `byte_size` (rounds up),
  `size` (rounds down), `binary_part/2,3`. Errors → `badarg`.

## Records

- Ordinary records expand to tuples `{Tag, Fields...}`. Declarations must
  precede use; duplicates, unknown fields, forward/self references and invalid
  wildcard fields are errors.
- Construction evaluates fields in declaration order: explicit value, else
  `_ = V` wildcard default, else declared default, else `undefined`. Each default
  is evaluated separately per use.
- Patterns check arity and tag, then only the listed fields. `#r.f` is the
  one-based index (tag at 1).
- Field access checks arity and tag; failure is `{badrecord, V}` in bodies and
  rejection in guards.
- `is_record(V, r)` uses the declared arity. `is_record/3` needs an atom tag and
  integer arity (non-positive → false; wrong types → `badarg`); an atom third
  argument is the native-record query and returns false. Guards require literal
  arguments.
- Update `Expr#r{f = V, ...}` evaluates the new values in source order, then
  `Expr`, then checks arity and tag (`{badrecord, Value}` on mismatch, also for
  `Expr#r{}`) and builds a new tuple; the other fields are copied. `_ = V` is
  rejected in updates; updates are illegal in patterns and guards.
- Not implemented: `record_info/2`, native/qualified/inferred forms.

## Comparison and order

Iterative with no work cap, as in OTP: only memory for pending pairs bounds a
comparison, map key searches included, and identical words are equal without
a walk. Byte-aligned bitstrings compare whole bytes at once. Order: numbers < atoms < tuples < maps < nil < lists <
bitstrings. Atoms compare by UTF-8 spelling (code-point order); tuples by arity
then fields; maps by size, then keys, then values; bitstrings by logical bits.

## Printing

`format_term` ([output.hpp](../runtime/include/erlang_aot/runtime/output.hpp))
renders any admitted term in one of two OTP styles. Integers, tuples (records
are tuples) and nesting look the same in both.

| | `~w` (`TermStyle::write`) | `erlang:display/1` (`TermStyle::display`) |
| --- | --- | --- |
| Atoms | Quoted unless a Latin-1 lowercase letter starts it and name characters (with `@`) follow; reserved words and `maybe`/`else` are quoted; beyond Latin-1 escapes as `\x{H}` | Quoted unless a Latin-1 lowercase letter starts it and alphanumerics or `_` follow; reserved words and `@` get no special rule; UTF-8 kept |
| Floats | Shortest round trip in OTP layout: `0.1`, `100.0`, `1.0e16`, `1.5e-7` | C `%.6e`: `1.500000e+00` |
| Lists | Elements: `[104,105]`, `[1,2\|3]` | A flat list of printable Latin-1 bytes prints as `"hi"` (raw bytes; only `\n` and `"` escaped) |
| Bitstrings | `<<1,2,5:3>>` | A printable ASCII binary prints as `<<"hi">>`, others as `~w` |
| Maps | `#{k => v,k2 => v2}` | `#{k=>v,k2=>v2}` |

- Maps print in map-key order (`maps:iterator(M, ordered)`, as OTP `~kw`). OTP's
  default `~w` and `erlang:display/1` follow its internal layout instead:
  atom-table order for atom keys of small maps (it varies between VM runs) and
  hash order above 32 keys. ErlangAoT does not reproduce that order.
- Rendering is iterative, so depth is limited only by the term. Text is capped
  at 64 MiB by default; exceeding it (for example a widely shared subterm) fails
  with `resource_limit` and returns no partial text.
- Goldens: `runtime_printing` compares both styles with OTP for 9,542 values (all
  corpus results plus authored edge cases); display rows whose OTP map order is
  internal are skipped ([fixtures](../tests/fixtures/printing/README.md)).
