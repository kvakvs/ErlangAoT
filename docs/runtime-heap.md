# Process heap contract

The runtime is moving its process heap to the classic ERTS design. This note is
the contract for that work; each item names the plan step that delivers it.
[runtime.md](runtime.md#process-memory) describes what runs today.

## Why the current heap is replaced

| Today | Problem | Replacement |
| --- | --- | --- |
| A list of chunks that never move | Cells cannot be compacted or copied; capacity only grows | One contiguous heap block plus fragments, moved by a copying collector (8G, 8H) |
| Each cell is a node in a per-process `std::map` index | Heap words alone are not parseable; one host allocation and an O(log n) lookup per cell | Self-describing cells; admission by owned range and header (8C, 8D) |
| Every bitstring cell has a fixed 64-byte array and a `shared_ptr`, released through a destructor registry | Large cells for small data; nothing can move a cell or find its dead copies | Variable-size heap binaries and off-heap binary cells on a per-process off-heap list (8B) |
| Host `Term` pins the heap with `shared_ptr<HeapStorage>`; runtime-held values live only in `Term`s | Nothing a collector can find or rewrite | ERTS model: C++ holds raw words only between safe points; runtime-held values are process root words; host callers pass explicit roots to `collect()` (8E) |
| One heap buffer per generated root frame | No process stack to scan | One stack of root frames per process (8F) |
| No overflow area | Allocation either fits the budget or fails | Heap fragments while the heap must not move (8G) |

Generated code, its ABI and every observable program result stay unchanged.

## Word layout

A term is one target word (32 or 64 bits); encodings are in
[abi.md](abi.md#terms). Every heap area is a sequence of objects that a walker
parses from its first word:

- **Header word** (primary tag `00`): bits 2–6 hold the `BoxedKind`, bits 7 and
  up the number of words that follow the header. A boxed term points at its
  header. The count covers every prefix, payload and padding word, so the walker
  skips untraced payload without interpreting it.
- **Cons cell**: two term words (head, tail) with no header. A list term points
  at the head. A head is never a header because no term has tag `00`.
- **Filler**: the all-zero word (kind `tuple`, count 0) is a one-word filler;
  kind `filler` with count n covers n further words. Reservations start zeroed,
  so reserved but unused words parse as filler. Nonempty tuples always have a
  nonzero count and `{}` is an immediate.

No cell needs alignment stronger than a word. `memory/heap_walk` parses an area cell by cell and `ProcessHeap::verify`
checks a whole heap (8C). Cells hold only words and bytes,
except the off-heap binary's `std::shared_ptr` (below), so a cell moves by
copying its words.

| Kind | Words after the header | Traced words |
| --- | --- | --- |
| cons (no header) | 2 words in total | head, tail |
| `tuple` | n element slots | all |
| `map` | 2n slots: keys in exact term order, each followed by its value | all |
| `bignum` | sign word, then magnitude limbs, least significant first | none |
| `floating` | 8 bytes: 1 word (64-bit) or 2 words (32-bit) | none |
| `heap_binary` | bit length, then data bytes rounded up to words (at most 64 bytes) | none |
| `refc_binary` | bit offset, bit length, `std::shared_ptr` (2 words), off-heap link: 5 words | none |
| `filler` | n unused words | none |

The `map` count is in words (entries = count / 2). Kinds not yet admitted
(references, funs, closures, native records, external identities) follow the
same rules when they arrive: identities and descriptors are registry IDs in
untraced words, never owning C++ pointers.

## Off-heap binaries

A binary larger than 64 bytes is an immutable buffer that floats outside every
process heap, shared by reference count (BEAM ProcBin and `Binary`).

- Its boxed `refc_binary` cell holds a `std::shared_ptr` to the buffer, a bit
  offset and a bit length; slices of a large binary are new `refc_binary` cells
  sharing the buffer (ERTS sub-binaries are not used). Copying a cell to another
  process copies the `shared_ptr`.
- Each cell is linked into its process's off-heap list through its link word.
  The list is the only way to find these cells' C++ state.
- Moving a cell copies its other words and move-constructs the `shared_ptr` into
  the new cell, so the old copy owns nothing. After a collection the list sweep
  relinks moved cells and destroys the `shared_ptr` of dead ones; teardown
  destroys all of them. A buffer is freed when its last cell dies.
- `std::shared_ptr` is two pointers on every supported STL; a `static_assert`
  keeps the cell size fixed at five words after the header on both widths.

## Areas

- **Heap.** One block `[start, top, end)` with bump allocation (8G). Until then
  the chunk list stays and every rule here applies per chunk.
- **Fragments.** When a request does not fit and the heap may not move, the
  runtime allocates a fragment sized to fit (at least the minimum heap size)
  and chains it to the process. The next collection merges fragments into the
  new heap block.
- **Stack.** Generated root frames (BEAM Y registers) are windows in stack
  segments kept apart from the heap (8F). A frame that does not fit the last
  segment opens a new one of 256 words (or the frame size if larger), and a
  segment is freed when its last frame returns. A frame never spans segments,
  so its address stays stable while generated code holds it. Bounds stay
  1,000,000 live words and 4,096 frames. This is a minimal interim form: once
  generated code reloads its frame base after safepoints (steps 17, 24, 26),
  the stack can become one flat array that moves as it grows.
- **Off-heap list.** As above.
- **Old heap.** None. Generational collection is deferred; immutable terms
  never point from older to newer data, so a high-water mark and an old heap
  can be added later without changing cells.

## Sizing and budget

- The heap starts at `min_heap_words` (233 words, as ERTS) and grows along the
  ERTS size sequence (Fibonacci-like up to about one million words, then 20%
  steps).
- After a collection the new block is the smallest size that keeps live data
  below 75% of it; a block less than 25% used shrinks to that size.
- One budget, `limit_bytes`, covers the heap block, fragments and the bytes of
  off-heap buffers created by this process. Exceeding it is `limit_exceeded`; a
  failed host allocation is `out_of_memory`. During a collection the old and new
  blocks coexist; only the new block is checked against the budget. The stack
  keeps its own root bounds.

## Admission

Pointers into a process heap are created only by the compiler and the runtime
inside that process, and always name an object start; there are no interior
pointers to detect. Admission (8D) is an ownership check for words handed back
to a process:

1. The address is word-aligned inside one of the process's areas, below its
   `top` (today: below a chunk's used words, found through chunks sorted by
   address). Foreign and stale words fail here without any load.
2. A boxed word names a header of an admitted kind (not filler); a list word
   names a cons cell (a word that is not a header).

Accessors decode kind, count and payload from the header itself. `verify()`
remains the full check that every slot names an object start, for tests.

## Roots and safe points

Roots are stack frame slots (8F), process root words for result handoffs (BEAM
X registers) and the current error payload (BEAM `fvalue`), the explicit root
span a host caller passes to `collect(roots)` (8E), and off-heap list links.
`ProcessContext::visit_roots` enumerates every root word for the collector. Atoms
and small immediates are not roots.

As in ERTS C code, a host `Term` is a raw tagged word valid until the next safe
point of its heap. It does not pin heap storage; it keeps a weak context
lifetime token and the heap's collection count, so use after teardown reports
`expired_context` and use after a later collection reports a stale-term error.
A `Term` is only valid inside its own process; other processes may only read
it.

The heap moves only at a safe point. Until generated code reloads values after
allocation (plan step 26), the only safe point is an explicit host `collect()`
while the context runs no generated code and has no pending reservation; any
other request returns `unsafe_point` and changes nothing. Allocation never moves
the heap before step 26: a request that does not fit creates a fragment.

## Collection

A full-sweep Cheney copy (8H): allocate the new block first (failure leaves the
heap untouched), copy objects reachable from roots, then scan the new block and
copy their children. A moved boxed object's header is replaced by a boxed
pointer to its copy; a moved cons cell gets a zero head and a tail pointing to
its copy. Forwarding preserves sharing. Roots are rewritten, the off-heap list
is swept, and the old block and fragments are freed. Statistics report heap,
fragment, stack and off-heap sizes.

## Baseline measurements

`runtime_heap_measurements` (full-mode CTest; numbers printed, not gated) builds
a 100,000-element list of `{Index, Float}` tuples through `TermFactory`, walks
it back through checked accessors, and creates 1,000 contexts that each hold
one small tuple. Side bytes are host allocations beyond heap backing (the object
index and other metadata).

| Revision | Build | Kernel build / walk | Heap used / capacity words | Side bytes | Bytes per context | Heap words per context |
| --- | --- | --- | --- | --- | --- | --- |
| `bb09359` (chunk list, object index) | Windows x64 Debug, clang-cl | 264 / 81 ms | 700,000 / 704,512 | 24,002,256 (about 80 per cell) | 66,217 | 8,192 |
| 8D (chunk list, owned range) | Windows x64 Debug, clang-cl | 185 / 147 ms | 700,000 / 704,512 | 3,440 | 66,057 | 8,192 |
