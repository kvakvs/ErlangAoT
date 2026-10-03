# Bitstring construction and matching

Bitstrings are immutable packed MSB-first sequences with an exact logical bit
length. Unused low bits in the final byte are zero. Binary classification depends
on a whole-byte logical length, including slices whose backing offset is not
byte aligned. Constructed values up to 64 bytes use inline process storage; larger values use
immutable shared buffers. Extracted large tails retain that buffer independently
of the source handle. All cells remain process-owned: foreign, interior and
expired words fail admission before any data access.

Construction evaluates source operands in order, then stages all segments in a
private bounded buffer. A failed segment publishes no partial bitstring. Only a
complete value reserves heap storage and publishes its owned cell/destructor.
Shared backing is charged against the creating process's heap budget; retained
views charge their own cells without charging the same backing again. A value
has a one-million-bit work/size ceiling. General cross-process copying, garbage
collection and worker synchronization remain deferred.

Integer segments truncate to the requested low bits, including arbitrary and
negative integers. Extraction supports signed and unsigned fields, explicit
big/little endian and native endian derived from the emitted LLVM data layout.
Little-endian partial fields use byte groups followed by any partial group.
Defaults and units are normalized after semantic modifier validation. Binary,
bytes, bitstring and bits aliases preserve their respective default units.

Float construction accepts numbers at widths 16, 32 and 64, rounds to the chosen
IEEE format and can encode infinity on narrow overflow. Extracted runtime floats
remain finite, so infinity and NaN fields reject a match. A zero-width float
extracts positive `0.0`, as OTP does; zero-width float construction is invalid.
Literal float patterns compare the decoded value with the original binary64
literal; integer literals are coerced to floats. Construction rounding does not
round a pattern literal. UTF-8, UTF-16 and UTF-32 share checked integer field
services and validate Unicode scalars, surrogate pairs, continuation bytes,
overlong sequences and truncation.

Function and body patterns carry an explicit encoded bit cursor. Each checked
service proves candidate ownership, type, size arithmetic and remaining bits
before reading or allocating. Only successful extraction advances the cursor.
Size expressions use incoming bindings plus permitted preceding segments;
compound sibling patterns retain separate incoming scopes. Unsized tails must
satisfy the segment unit, and the final cursor must consume the entire candidate.
An explicit `:all` expression is an invalid runtime size in both construction
and patterns; only an omitted binary size supplies the unsized-tail default.

Exact/numeric equality and term order compare logical bits independently of
buffer identity, offsets and padding. Bitstrings sort after lists and may serve
as exact map keys. `is_binary/1`, `is_bitstring/1`, `bit_size/1`, `byte_size/1`,
`size/1` and `binary_part/2,3` use the same checked representation. `byte_size/1`
rounds up; `size/1` rounds down. Binary parts use checked byte indices and allow
negative lengths.

Ordinary construction/query errors raise `badarg`. Segment rejection in a head
tries the next clause; rejection in a body match raises `badmatch` with its
rooted original RHS. Guard semantic errors reject their alternative. Ownership,
allocation, resource and internal failures propagate through the existing checked
call channel and cannot become selection outcomes. Failed candidates discard
their temporary roots. Until GC exists, cells remain in the owning heap backing;
teardown invalidates access, and release of the final heap/host pin destroys the
cells and shared buffers. Both successful extraction
outputs and every borrowed argument occupy the shared generated root frame.

The project-owned [bitstring goldens](../tests/fixtures/patternmatch/generated/bits/manifest.json)
retain oracle versions, calls and expected values/errors, with hashes of the
[locally authored source](../tests/fixtures/patternmatch/fragments/bits/answer.erl).
Routine tests require no OTP installation or source checkout. See
[step-16 validation](patternmatch-step16-validation.md) for native, parser,
foreign-object, ownership and failure evidence. Records and remaining guard
families retain their separate plan steps.
