# Parser validation — 2026-09-19

Phase VI implementation and local validation are complete. The full parser plan
remains **partially complete** because required Linux and Windows host execution
is unavailable in this workspace. No cross-host success is inferred from macOS.

## Reference and coverage

The grammar source is OTP 29.1, revision
`751f87b703fe5948607d08e82599ce644b772e76`. Live structural comparisons use the
installed Homebrew OTP 29.0.5 selected by CMake. The reduction audit separately
generates a parser from the pinned 29.1 grammar and measures actual reductions.

- All 344 ordinary grammar productions have observed fixture witnesses; 79 SSA
  annotation productions are explicitly excluded. The 423-row inventory has no
  pending ordinary rows. This measures grammar reductions, not every helper branch.
- All 43 positive authored `.erl` fixtures have native/OTP structural comparisons,
  including 14 original seeds promoted from reference-only coverage. The corpus
  also has 183 ordinary rejection fixtures, three explicit OTP builder-exception
  fixtures and the original `bad.erl` error-location/class reference.
- Native variant visitors are exhaustive; unsupported OTP projection nodes raise
  errors. Canonicalization deliberately removes grouping/annotations and normalizes
  OTP list spines, while native tests protect retained syntax/provenance separately.
- Unknown/unmapped node kinds are never silently dropped. Builder exceptions are
  limited to the named `record_helper`, `record_extra` and `any_first` fixtures;
  the native parser returns recoverable diagnostics for them. Syntax acceptance
  remains distinct from lint; no full upstream Common Test run is claimed.

Witnesses and source hashes are in
[`tests/fixtures/parser/phase6`](../tests/fixtures/parser/phase6/README.md).
The original reference records and their checksum file were not changed.

## Real pinned OTP source corpus

All ten entries below passed preprocessing, parsing and repeated AST-tree
comparison. Failures are reported by stage; no corpus exceptions or skips were
needed. The manifest records SHA-256 values and the runner verifies a clean
relevant checkout at the pinned revision.

| Sources | Coverage emphasis | Result |
| --- | --- | --- |
| stdlib `lists.erl`, `maps.erl`, `sets.erl` | clauses, comprehensions, specs, maps | Passed |
| stdlib `erl_scan.erl` | scanner's own Erlang syntax, guards, macros | Passed |
| compiler `beam_ssa.erl`, `beam_asm.erl` | records, includes, bit syntax, specs | Passed |
| compiler `beam_ssa.hrl`, `beam_asm.hrl`, `beam_opcodes.hrl`, `beam_types.hrl` | declarations, type syntax, macro definitions | Passed |

Options: `-I <otp>/lib/compiler/src`, explicit stdlib/kernel `--app-dir` mappings,
`--enable-feature maybe_expr`, `--disable-feature compr_assign`, and
`-DCOMPILER_VSN='"parser-compatibility"'`. OTP normally supplies that version macro
from its build. No source is rewritten. Results and local tree hashes are emitted
to `<build>/tests/corpus/corpus.tsv`; hashes include presentation/source information
and are not a portable serialization contract.

## Executed build matrix

Host: macOS 26.6.2 (25G83), arm64. Toolchain: Apple Clang 21.0.0
(`clang-2100.1.1.101`), CMake 4.4.2, Homebrew Boost 1.92, OTP 29.0.5.

| Configuration | Directory | Evidence |
| --- | --- | --- |
| Full C++23 Debug | `build/debug` | All 46 CTests and fresh full Lizard/clang-tidy gate passed |
| Full C++26 Debug (`-std=c++26`) | `build/phase6-cxx26` | Build and all 46 CTests passed across full run and focused rerun |
| Full C++23 ASan + UBSan | `build/phase5-sanitize` | Build and all 46 CTests passed across full run and focused rerun; no sanitizer findings |
| Compiler-only C++23 Debug | `build/phase6-compiler-only` | Build and all 46 CTests passed |
| Runtime-only C++23 | `build/phase6-runtime-only` | Configure/build passed; no compiler/OTP/Boost dependency discovery |
| Linux x86-family | Unavailable | Pending execution |
| Linux ARM | Unavailable | Pending execution |
| Windows x86-family | Unavailable | Pending execution |

The focused reruns corrected escaping of semicolons/unmatched brackets in the new
CMake inventory checker, then reran both historical suites and the corpus with
explicit feature options. No compiler findings were bypassed. No source-dependent
tests skipped on this host. Required cross-host jobs remain open work; the
repository has no available configured CI runner for those targets in this session.

The 12,000-operator Debug parse/print/normalization regression took approximately
0.7 seconds on this host. Generated tests replay 900 mutations twice with fixed
seed `0x29a016`, compare diagnostics/trees, and retain a following good form.
Large flat lists and qualifiers, recursive patterns/types/blocks, resource errors,
printing and teardown are covered. This is bounded stress evidence, not a claim
of linear runtime or a wall-clock guarantee for arbitrary source sizes.

## Reproduce

```sh
cmake --preset debug
cmake --build build/debug --parallel 2
ctest --test-dir build/debug --output-on-failure
cmake --build build/debug --target check-quality

cmake -S . -B build/phase6-cxx26 -DCMAKE_BUILD_TYPE=Debug -DERLANG_AOT_CXX_STANDARD=26
cmake --build build/phase6-cxx26 --parallel 2
ctest --test-dir build/phase6-cxx26 --output-on-failure

cmake -S . -B build/phase5-sanitize -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/phase5-sanitize --parallel 2
ctest --test-dir build/phase5-sanitize --output-on-failure

cmake -S . -B build/phase6-compiler-only -DCMAKE_BUILD_TYPE=Debug -DERLANG_AOT_BUILD_RUNTIME=OFF
cmake --build build/phase6-compiler-only --parallel 2
ctest --test-dir build/phase6-compiler-only --output-on-failure
cmake -S . -B build/phase6-runtime-only -DERLANG_AOT_BUILD_COMPILER=OFF
cmake --build build/phase6-runtime-only --parallel 2
```

Set `ERLANG_AOT_OTP_SOURCE_ROOT` if the ignored checkout is elsewhere. Without
the pinned checkout, only `parser_coverage` and `parser_corpus` explicitly skip;
their work is then pending, not passed. Native compiler tests still require a
working installed OTP >=29, supplied through `ERLANG_AOT_ESCRIPT` if necessary.

Duplication review: the parser uses the existing lexer/expanded tokens, shared
cursor/operator/diagnostic/delimiter helpers, exact literal values and binary
encoder. Syntax trees remain distinct from the preprocessing evaluator. Token
printing is shared with stringification; CLI modes share one frontend driver.
No stage reader, backend, semantic analysis or new public serialization was added.
