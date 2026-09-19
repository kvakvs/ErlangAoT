# Parser implementation and validation

Baseline: OTP 29.1 (`751f87b703fe5948607d08e82599ce644b772e76`).

Current native test builds discover installed OTP 29+ at CMake configure time and
fail for missing/older installations. Live suites run on that selected version;
the source-dependent reduction audit separately verifies the pinned grammar.

Implementation follows [.agents/02-parser.md](../.agents/02-parser.md).

## Using the parser

`erlangaot --parse-check -I include -DDEBUG module.erl` runs preprocessing and
syntax parsing with diagnostics on stderr and no stdout on success. All existing
preprocessing options apply, including application paths and feature selection.
Combining it with `--preprocess-check` still parses; `--print-pp` and `--print-ast`
request source/tree output from that same preprocessing pass. Output-path options
are usage errors in any check/print mode. Multiple inputs have isolated state and
retain a failing exit status when an earlier input fails. `--` permits paths that
start with a dash. Exit codes are 0 for success (including warnings), 1 for source/
filesystem failures, and 2 for usage errors. Check modes do not write output files.

The public entry point is `erlang_aot/compiler/parser.hpp`, linked through the
`erlang_frontend` CMake target:

```cpp
erlang_aot::ParseResult load(const std::filesystem::path &path) {
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession pp(sources.read(path));
    return erlang_aot::parse_module(pp);
}
auto result = load("module.erl"); // Sources and preprocessing session are gone.
for (const auto &id : result.module.forms()) {
    const auto &form = result.module.form(id);
    const auto &origin = result.module.anchor(form.source);
    // origin.location is logical; origin.spelling and related retain physical sources.
}
```

`ast::Module` is move-only and owns completed forms, flat node arenas, source
buffers and immutable feature snapshots. Moving it preserves handles. Distinct
`ExprId`, `PatternSyntaxId`, `TermId`, `TypeId` and `FormId` types prevent mixing
categories; owner/generation checks reject foreign or stale handles. Node references
are valid for the lifetime of the finished owner. `module.visit(id, visitor)`
dispatches a category's closed variant; exhaustive visitors must handle each
alternative. Consumers should use explicit stacks for arbitrarily deep flat trees.
`tests/compiler/parser/consumer.cpp` is a compiled post-session ownership example.

`NodeSource` is a half-open expanded-token range and anchor within an owned
per-form origin table. Use `extent(source)` and `anchor(source)`, never assume
a node's macro-expanded spelling is contiguous in one physical file. Per-form
`features(id)` captures the state at emission; `features()` captures final state
at EOF, or state at resource exhaustion if parsing stopped early.

The implemented grammar includes ordinary attributes and normalized literal terms,
tuple/native records, type/opaque/nominal declarations, specs/callbacks, all ordinary
expressions, restricted and candidate patterns, guards, clauses, binary syntax,
fun/try/maybe/control flow, and list/map/binary comprehensions with OTP 29 templates,
strict arrows and zip groups. SSA test annotations are outside ordinary source
grammar. Binding, guard legality, record/type resolution, lint, parse transforms,
documentation-file ingestion by the parser, LLVM lowering and execution are later
stages. Syntax success does not promise any of them. Feature context is retained
for those later stages rather than inferred from printed source.

For streaming consumers, `ParserSession::consume(event)` accepts expanded forms
and diagnostics; an unexpected directive or missing semantic feature context is
a contract diagnostic. `parse_form(tokens, eof, features)` is the lower-level raw
token API. `std::move(session).finish(features)` transfers ownership exactly once.
Always inspect `ParseResult::failed`: recovered forms do not imply module success.

Step 17 validation: all 40 Debug CTests passed, including the expanded CLI checks
and post-session consumer. Those two tests also passed under ASan/UBSan. All 14
successful historical parser fixtures matched the live OTP 29.0.5 structural
projection. Five real pinned OTP modules (`lists`, `maps`, `sets`, `erl_scan`,
`beam_ssa`) passed the new CLI using explicit include/application paths.
Fresh full Debug configuration, formatting, Lizard and clang-tidy passed on macOS arm64.

## Phase VI step 16 — Recovery and resource contracts

Syntax diagnostics retain a stable category, logical invocation coordinates,
physical macro/include traces and, where available, the nearest unmatched opener.
Expected terminals/categories are also available in `Diagnostic::expected`.
Raw expanded-token callers supply an explicit EOF token, including its provenance.
Delimiter recognition is shared with macro argument splitting.

Each failed form rolls back all arenas and origins. Subsequent complete forms
remain available, but failure is sticky. Resource exhaustion stops the session.
Defaults are 1,000,000 tokens/form, 4,000,000 tokens/module, 1,000,000 nodes across
all arenas, 1,000 diagnostics plus one exhaustion message, recursive depth 256 (hard ceiling 512), and 16,000,000 work
units. Work accounts for input tokens,
grammar entries, node creation, normalized literal contents and map insertion/
metadata sorting. These are accounting limits, not wall-clock guarantees or a
claim of linear runtime; decoded source sizes and shared binary literal limits
also affect cost. Preprocessing has its own independently configurable limits.

The public tree printer uses an iterative work queue, caps displayed indentation,
and defaults to 4,000,000 visited objects. Its optional third argument changes
that budget; exhaustion throws `std::length_error`. Flat arena destruction
does not recurse through child IDs. Consumers should also traverse iteratively
when following long flat operator chains.

`parser_hardening` covers a 12,000-operator parse/print/normalization regression,
wide lists and qualifier groups, recursive expressions/patterns/types/blocks,
repeated errors, explicit expanded EOF and macro/include origins. It reports an
observed duration without asserting an asymptotic bound. `parser_mutations`
replays 900 fixed-seed token mutations twice, verifies deterministic diagnostics
and checked traversal, and requires the next valid form to survive. Both tests
have 60-second termination bounds and run under ASan/UBSan as well.

Step 16 validation: all 39 Debug tests passed across the full run and corrected
hardening rerun; the three ASan/UBSan hardening/mutation/printing tests passed.
Fresh full Debug configuration, formatting, Lizard and clang-tidy passed on
macOS arm64 with installed OTP 29.0.5. The 12,000-operator regression took about
0.7 seconds in Debug; this is one host observation, not a scaling guarantee.

## Compatibility evidence

See [the current validation matrix](parser-validation.md) for measured grammar
coverage, the pinned real-source corpus, reproducible commands and pending hosts.
Earlier phase-by-phase implementation records remain in Git history. The original
reference files remain protected by their existing checksums.
