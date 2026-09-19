# TOML projects and target selection implementation plan

Status: steps 1–10 complete; steps 11–22 pending. Written 2026-09-19.

Validation ledger: step 1 — fresh Debug compiler/runtime build, all 46 CTest
tests, Lizard, clang-tidy, TOML documentation examples, and whitespace checks pass.
Step 2 — all 47 tests and full quality pass; runtime-only build and absent/changed
TOML-root configuration checks pass. Pinned toml++ 3.4.0 is local under build/deps.
Step 3 — owned model defaults/move/isolation tests, all 48 tests, formatting,
fresh Debug build, Lizard, and clang-tidy pass.
Step 4 — loader native-path/syntax/size/injected-I/O tests, all 49 tests,
formatting, fresh Debug build, Lizard, and clang-tidy pass.
Step 5 — strict source-declaration schema, ownership, and count-limit tests,
all 50 tests, formatting, fresh Debug build, Lizard, and clang-tidy pass.
Step 6 — nested typed-option/feature-conflict tests, all 51 tests, formatting,
fresh Debug build, Lizard, and clang-tidy pass.
Step 7 — path bases, ordered fallback, invalid candidates, Unicode and symlink
tests; all 52 tests, formatting, fresh Debug build, Lizard, and clang-tidy pass.
Step 8 — wildcard/Unicode/work-limit tests, all 53 tests, formatting, fresh Debug
build, Lizard, and clang-tidy pass. All 53 tests also pass after localizing project
test CMake ownership; production translation units are unchanged by that move.
Step 9 — bounded traversal, matching budgets, symlink policy and literal manifest
directory tests; all 54 tests, formatting, fresh Debug build and full quality pass.
Step 10 — ordered source assembly, overlapping selections and physical aliases;
all 55 tests, formatting, fresh Debug build and full quality pass. All 55 tests
also pass with native case-alias coverage added.

## Objective and existing behavior

Add a TOML project manifest describing one or more named compilation targets,
their sources, and per-target compiler configuration. Accept it through
`--project <path>` and select targets through repeatable `--target <name>`.
Add `--new-project <filename>` to create an annotated TOML starter containing
one target with default settings.

The current driver in `compiler/src/{main.cpp,driver/}` already accepts multiple
positional source files and supports `--preprocess-check`, `--parse-check`,
`--print-pp`, and `--print-ast`. Preserve those contracts, including single-source
usage, per-module isolation, option ordering, and exit codes. Project support
must work with all four frontend modes. Executable generation remains
unimplemented and must still fail without creating or modifying output files.

This plan adds project configuration and orchestration, not LLVM lowering,
linking, runtime implementation, or a public compiler-stage interchange format.
Keep `compiler/src/stage_readers/{preprocessed,abstract,ir}/` reserved only.

## Proposed version 1 contract

These choices make the implementation actionable; they are proposed project
policies, not existing behavior or claims of compatibility with another build tool.

### Manifest

```toml
schema_version = 1

[[targets]]
name = "app"
sources = ["main.erl", "src/workers/*.erl", "shared/**/*.erl"]
source_dirs = ["src/support"]
output = "build/app"

[targets.options]
source_search_paths = ["src", "generated"]
include_dirs = ["include", "vendor/include"]
defines = ["DEBUG", "LIMIT=100", 'LABEL="demo"']
enable_features = ["maybe_expr"]
disable_features = ["compr_assign"]

[targets.options.applications]
my_dependency = "vendor/my_dependency"

[[targets]]
name = "tests"
sources = ["tests/*_tests.erl", "src/**/*.erl"]
output = "build/tests"

[targets.options]
include_dirs = ["include", "tests/include"]
defines = ["TEST"]
```

- Require integer `schema_version = 1` and a nonempty `targets` array of tables.
  Reject unsupported versions, unknown keys, wrong value types, duplicate target
  names, and empty path/name/definition strings. No coercion of integers or booleans
  into strings. Validate the complete manifest schema, including unselected targets.
- Names are case-sensitive ASCII `[A-Za-z0-9_][A-Za-z0-9_.-]*`, unique per manifest.
  This name identifies a build target; it is not an LLVM architecture triple.
- `sources` and `source_dirs` are optional string arrays, but each target must
  contain at least one entry across them. `sources` accepts literal filenames
  and wildcard patterns. `source_dirs` accepts literal directory names and
  recursively discovers `.erl` files. Headers are dependencies, not translation
  units. Explicit files must also have the `.erl` extension.
- `options` and its fields are optional; omitted arrays/maps are empty and retain
  frontend defaults. These typed fields are the initial compile options. Do not
  accept arbitrary command strings or pretend to support optimization, backend,
  target-architecture, linking, warning-policy, or parse-transform settings yet.
- `defines` uses the existing `NAME` / `NAME=ERLANG_LITERAL_TERM` contract;
  `NAME` means `true`. Use the existing lexer/preprocessor validation, not a
  second Erlang parser. Reject duplicate macro names rather than silently
  replacing definitions. A feature cannot appear in both manifest feature lists.
- Optional `output` reserves the future executable destination. Its default is
  `<manifest-dir>/build/<target-name>` (`.exe` on Windows). Check/print modes ignore
  manifest output destinations and never create their directories. Future backend
  work may revise defaults with a documented schema/compatibility decision.
- Root defaults, inheritance, target dependencies, project imports, profiles,
  exclusions, package fetching, watch mode, caching, and parallel compilation are
  deferred. Add schema fields when their behavior is implemented, not as ignored
  placeholders.

### CLI and target selection

```text
erlangaot [options] <source.erl>...
erlangaot [options] --project <path> [--target <name>]...
erlangaot --new-project <filename>

erlangaot --new-project example
erlangaot --parse-check --project example.toml
erlangaot --print-ast --project example.toml --target app
erlangaot --parse-check --project example.toml --target tests --target app
```

- Positional sources and `--project` are mutually exclusive. Preserve existing
  multiple-positional-source support; the requested single-source form remains
  a special case. Do not infer projects from positional filename extensions.
- Accept exactly one `--project` operand and any number of `--target` operands,
  in any option order. Reject missing/empty operands, repeated `--project`,
  `--target` without a project, and mixed project/positional inputs. Preserve
  `--` and current option-operand consumption behavior; do not add `--name=value`
  spellings as part of this change.
- Without `--target`, select all targets in manifest order. With selectors,
  preserve selector order, keeping only the first occurrence of each name.
  Unknown names fail before frontend processing and list available names in
  manifest order. No default-target field or implicit dependency expansion.
- Validate CLI syntax before honoring help/version, as today, but help/version
  do not open/create the project or resolve target names. Keep help precedence
  over version.
- Existing CLI frontend options apply to every selected target. CLI-relative
  paths retain invocation-working-directory meaning; manifest-relative paths
  retain manifest-directory meaning, regardless of option position.
- Explicit `-o/--output` remains illegal in check/print modes. For a compilation
  request it can override project output only when exactly one target is selected;
  reject it for multiple selected targets. Track explicit output presence rather
  than guessing from the existing `a.out` default. Keep positional-mode defaults.
- Exit `2` for CLI usage errors and unknown target selectors; exit `1` for
  manifest I/O/TOML/schema errors, source discovery errors, frontend failures,
  project-creation failures, and the unimplemented backend; exit `0` for successful
  checks/printing, project creation, or informational output. Warnings alone remain
  successful.

### Creating an annotated project

- `--new-project <filename>` is a standalone creation command. Resolve its path
  against the invocation directory. Append `.toml` unless the final component
  already ends in `.toml` (ASCII case-insensitive); preserve an existing suffix's
  spelling. Thus `demo` becomes `demo.toml`, `demo.toml` stays unchanged, and
  `demo.config` becomes `demo.config.toml`. Do not replace another extension.
- Reject an empty operand, a path without a filename (including a trailing
  separator or final `.`/`..`), and repeated `--new-project`. Reject combinations
  with positional sources, `--project`, `--target`, `-o`, any check/print mode,
  or any explicit frontend configuration option, even if its value is a default.
  Help/version are allowed and prevent file creation after CLI validation.
- Create only the requested manifest. Require its parent directory to exist;
  do not generate source files, source directories, or output directories. Refuse
  to overwrite any existing destination, including a directory or symlink.
  Use exclusive creation, not a separate existence check followed by truncation.
  Report write/close failures, clean up only the incomplete file created by this
  invocation where possible, and never report success for a partial write.
- Emit deterministic UTF-8 text with LF newlines, a final newline, and comments
  explaining each supported field, alternative source selection, target selection,
  and frontend usage. Do not embed timestamps or absolute paths. On success print
  the created path to stdout and leave stderr empty; failures use stderr and
  the exit codes above.
- Use exactly one target named `app`, independently of the manifest filename.
  Use `sources = []` and `source_dirs = ["src"]` as the starter source layout;
  this is a template choice, not a new implicit source default for other manifests.
  All optional option arrays/maps are explicitly empty, retaining frontend defaults.
  Write the effective default output `build/app` (`build/app.exe` on Windows).
  Do not enable example macros, feature overrides, include paths, or applications.
- The generated document must pass TOML/schema decoding immediately. Creation
  does not perform source discovery or compilation: the user supplies `.erl`
  files under `src` or edits source selection before checking the project.
  Empty feature lists mean no overrides, not that every feature is disabled.

Canonical starter content below shows the non-Windows output spelling; only the
output suffix differs on Windows. Keep these annotations in the generated file:

```toml
# ErlangAoT project. Paths are relative to this TOML file's directory.
# Check with: erlangaot --parse-check --project <this-file.toml>
# Also available: --preprocess-check, --print-pp, and --print-ast.
schema_version = 1

# Add another [[targets]] block after this target's option tables for more targets.
# All targets run by default; select with --target app (repeat for more names).
[[targets]]
name = "app"

# List files or patterns, e.g. ["main.erl", "src/*.erl", "shared/**/*.erl"].
# Patterns support *, ?, and ** as a whole directory component.
sources = []
# Recursively collect .erl files. Create/populate src or edit these selections.
# Set source_dirs = [] when selecting files exclusively through sources.
source_dirs = ["src"]
# Future executable path; checks/printing do not write it. Windows: build/app.exe.
output = "build/app"

[targets.options]
# Fallback roots for listed source filenames, e.g. ["src", "generated"].
# These do not discover extra files or search for headers.
source_search_paths = []
# Header search directories, first listed first, e.g. ["include", "vendor/include"].
# CLI -I directories take precedence; the last CLI -I is searched first.
include_dirs = []
# Predefined macros, e.g. ["DEBUG", "LIMIT=100", 'LABEL="demo"'].
# A bare name means true; values are Erlang literal terms. Duplicates are errors.
defines = []
# Empty lists preserve compiler feature defaults; CLI feature settings apply last.
# Example overrides: enable_features = ["maybe_expr"].
# Do not list a feature in both arrays.
enable_features = []
disable_features = []

# Explicit include_lib application roots. Empty by default.
# Example entry: my_dependency = "vendor/my_dependency"
[targets.options.applications]
```

### Paths and source discovery

- Resolve the supplied project path relative to the invocation directory and
  normalize it to an absolute path. Its lexical parent is the manifest base,
  including when the project filename is a symlink. Do not change process cwd.
- All manifest paths, including include/application/search/output paths, use
  that base; absolute paths remain absolute. Permit `..` and external source
  trees. No shell execution, environment-variable expansion, or tilde expansion.
  Document `/` for portable TOML paths and literal TOML strings for Windows paths.
- A literal relative source is tried against the manifest base first. Only if
  absent there, try `source_search_paths` in listed order and take the first
  existing file. An existing but unreadable/non-regular candidate is an error,
  not a reason to fall through. Absolute sources bypass search paths.
- Search paths locate explicitly listed source files only: they do not enumerate
  modules, participate in header lookup, or provide BEAM-style module loading.
  `source_dirs` and wildcard patterns resolve directly against the manifest base;
  they do not use source-search fallback.
- Implement portable `*` and `?` within a path component and `**` as an entire
  component matching zero or more directory levels. Bracket classes, braces,
  negation, and wildcard escaping are outside v1; reject those pattern constructs
  explicitly. Literal files with wildcard syntax in their names are outside v1.
  Match case-sensitively without locale folding; literal filesystem lookup still
  follows host filesystem rules. `?` matches one Unicode scalar in a valid UTF-8
  filename. Report an unsupported filename encoding rather than lossy conversion.
- Include hidden entries under explicitly selected roots, with no implicit
  `.git`/build/vendor exclusions. Skip discovered directory symlinks to prevent
  recursive cycles; explicitly named directory roots may be symlinks. Permit
  regular-file symlinks and report dangling selected links or traversal failures.
- Preserve `sources` entry order; sort each pattern's matches by normalized
  generic UTF-8 path bytes. Then append each `source_dirs` expansion in listed
  order, sorting each expansion the same way. Collect `.erl` regular files only.
  Missing literal sources/directories, unmatched patterns, or a target resolving
  to zero files are errors. An empty source directory is allowed if other entries
  supply files. Report traversal permission/I/O failures rather than skipping them.
- Deduplicate within each target using resolved filesystem identity, keeping the
  first display spelling and order. Cover symlink aliases, hard links, and native
  case aliases without lowercasing all paths. The same file in different targets
  must run separately because its options can differ.
- Decode all target configuration, then select targets, then resolve filesystem
  paths for selected targets only. An absent source tree belonging exclusively
  to an unselected target must not break a selected-target build.

### Effective options and execution

| Setting               | Manifest meaning and CLI combination                                                                                                                                          |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include_dirs`        | Listed search order; prepend the existing CLI `-I` search vector, whose last supplied entry is first. Preserve the preprocessor's built-in include lookup around that vector. |
| `source_search_paths` | Listed fallback order for project source discovery only; no new positional-mode search option in v1.                                                                          |
| `defines`             | Append CLI definitions after manifest definitions; duplicate names, including across the boundary, retain the existing redefinition error.                                    |
| `applications`        | Manifest application map, then CLI application entries replace matching names.                                                                                                |
| Feature lists         | Apply manifest enable/disable settings, then ordered CLI feature changes; CLI's final setting wins.                                                                           |
| `output`              | Per-target future destination, overridden by explicit CLI output for one selected target only.                                                                                |

Set each project frontend session's working directory to the manifest base;
normalize CLI-relative include/application paths against the invocation directory
before combining them. Preserve the existing include-resolution algorithm and
positional-mode behavior. Never share mutable preprocessing or parsing sessions
between files or targets.

Construct a fully validated ordered invocation plan before processing any source.
Project/schema/discovery errors prevent execution of that invocation. Detect
duplicate effective output destinations for compilation requests, including
existing filesystem aliases and normalized not-yet-existing paths; check modes
do not care about output collisions. No output is written during planning.

Once execution starts, process every selected target/file sequentially and latch
failures as the existing multi-input driver does. Project diagnostics include
manifest path, target name, and source context; manifest diagnostics include
line/column and key where available. Preserve frontend logical/physical origins.
Print source/AST payloads to stdout in target/file order using existing printers;
do not insert target banners into those formats. Repeated source output across
targets is intentional. Keep context on stderr when diagnostics occur.

## Architecture and dependency boundary

```text
CLI Options → project load/decode → target selection → path/source discovery
            → effective per-target invocation plan → existing frontend per file
positional Options ───────────────────────────────→ same execution boundary
--new-project → annotated template → exclusive manifest file creation
```

- Add compiler-private `compiler/src/project/` modules for manifest models,
  decoding, paths, matching, discovery, selection, planning, execution, project
  CLI handling, and diagnostics. Keep project-specific implementation and private
  headers in this directory as far as possible. In the steps below, `project/`
  means `compiler/src/project/` and `driver/` means `compiler/src/driver/`.
  Separate pure transformations from filesystem operations. Retain manifest key
  locations in owned records; no references into a destroyed TOML tree.
- Keep project option records, operand parsing, combination validation, and help
  text in `project/cli.{hpp,cpp}`; command orchestration belongs in
  `project/command.{hpp,cpp}`. Existing `driver/options.{hpp,cpp}` should only
  carry the project request and delegate project options/validation. `main.cpp`
  should only compose help and dispatch the selected command. Do not add
  `driver/project.*` or spread project policy through generic driver helpers.
- Keep target iteration in `project/execution.{hpp,cpp}` and project-specific
  diagnostic formatting in `project/diagnostics.{hpp,cpp}`. The generic driver
  retains positional iteration and shared per-file frontend processing. Extract
  a small per-file request/executor boundary and pass execution/diagnostic
  callbacks into the project command; do not make the project library depend on
  concrete driver implementation or mutable global CLI `Options`. Reuse existing
  frontend diagnostic rendering for logical/physical origins.
- Keep starter rendering in `project/template.{hpp,cpp}` and destination naming/
  exclusive writing in `project/create.{hpp,cpp}`. Creation is independent of
  source discovery and frontend execution; `project/command.cpp` handles that
  branch after generic help/version handling.
- Use toml++ through a private adapter and private CMake target. Its upstream
  documentation provides parsing/error and CMake integration APIs; pin and record
  a tested release and license during dependency integration. See the
  [upstream documentation](https://marzer.github.io/tomlplusplus/v3.4.0/index.html)
  and [repository](https://github.com/marzer/tomlplusplus). The project schema is
  deliberately limited to TOML 1.0 syntax. Do not handwrite a TOML parser.
- Prefer installed or explicitly provided local dependency roots. Configuration
  must not download dependencies automatically. Give an actionable missing-package
  diagnostic. Runtime-only builds must not discover or link TOML or frontend
  dependencies. Do not expose TOML types through public compiler headers.
- Own project source lists and private library wiring in `project/CMakeLists.txt`,
  with dependency discovery in `project/cmake/Dependencies.cmake` and any bundled
  template resources under `project/`. The parent `compiler/CMakeLists.txt` only
  adds this subdirectory and links its private target. Ensure the existing quality
  checks still cover all new project C++ translation units.
- Keep tests and user-facing artifacts in the repository's conventional locations:
  `tests/compiler/project/` for unit tests and `cli.cmake` integration tests,
  `tests/fixtures/project/` for fixtures, `docs/` for documentation, and
  `examples/project/` for examples. These and minimal parent build/driver wiring
  are intentional exceptions to project implementation locality. Do not move
  shared frontend code into `project/` merely because project execution uses it.

## Gate required at the end of every step

Each numbered step is one focused implementation commit. Its associated tests,
minimal build wiring, and directly relevant documentation belong in that commit;
unrelated refactors do not. Do not accumulate several steps into one commit.

1. Add/run the focused acceptance tests listed in the step. Keep test fixtures
   hermetic and offline; project behavior does not require new OTP oracle tests.
2. Format changed C++ with the repository clang-format configuration (`make format`
   or targeted `clang-format -i --style=file`). Review the diff and avoid unrelated
   formatting churn. Use `clang-format --dry-run --Werror --style=file` on changed
   C++ files to verify formatting; review Markdown/CMake/TOML layout separately.
3. Reconfigure and run the existing full checks, sequentially:

   ```sh
   cmake --preset debug -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
   cmake --build --preset debug
   ctest --preset debug --no-tests=error
   cmake --build build/debug --target check-quality
   git diff --check
   ```

   This is a fresh configuration before each commit, not a requirement to delete
   build caches. Both Lizard and clang-tidy must pass. Keep existing thresholds
   and suppressions unchanged; split helpers/modules to reduce complexity.
   Do not rebuild an executable while its CTest invocation is running.
4. Document each new function's intent and each new class field's purpose in
   one or two comment lines. Update `.agents/files.md` for file ownership changes,
   `.agents/arch.md` after architectural changes, and concise `aimemory.md` notes
   as useful. Record actual validation evidence and any unavailable host checks.
5. Only after the focused tests, full tests, formatting, and quality gate pass,
   create the step's named commit. If a gate fails, fix this step before continuing.
   Documentation-only steps also finish with this gate; no meaningless new tests
   are needed merely to exercise prose.

## Ordered implementation steps

### Step 1 — Publish the project format contract

Scope: `docs/projects.md` only, plus its file-map entry.
Transfer the v1 schema, CLI rules, path/glob behavior, option precedence, output
policy, and exit-code table above into user-facing documentation marked planned.
Include the two-target example and explicitly document retained positional lists.
Include the creation command, extension/no-overwrite rules, and annotated default
starter, distinguishing generation from checking a populated source tree.

Acceptance: review examples against every schema field and verify relative links;
confirm no README claim suggests the feature already works. Run the common gate.
Commit: `docs(project): specify version 1 manifests and invocation rules`.

### Step 2 — Integrate the private TOML dependency

Scope: `project/cmake/Dependencies.cmake`, `project/CMakeLists.txt`, minimal parent
compiler CMake wiring, dependency setup documentation, and a private TOML smoke test.
Pin a tested toml++ release and record provenance/license. Provide installed/local
discovery with consistent configuration macros and no network fetch at configure
time. Keep vendor headers outside project-source quality scans using a proper
external dependency boundary; project adapter code remains fully checked.

Acceptance: parse an in-memory valid/invalid TOML sample; compiler configuration
fails clearly when TOML is unavailable; runtime-only configuration/build succeeds
without discovering it. Run the common gate.
Commit: `build(project): add private tomlplusplus dependency`.

### Step 3 — Add owned project configuration types

Scope: `project/model.hpp`, `project/CMakeLists.txt` private library wiring,
test-target registration, and model tests.
Define project, target, option, located-value, and project-diagnostic records with
owned strings/paths and explicit optional output. Keep unresolved configuration
separate from resolved invocation data. Do not add parsing or filesystem logic.

Acceptance: defaults, ownership after moves, and construction of independent
target configurations; no TOML type in the model interface. Run the common gate.
Commit: `feat(project): add owned manifest configuration model`.

### Step 4 — Load TOML and translate syntax diagnostics

Scope: `project/loader.{hpp,cpp}`, `project/diagnostics.{hpp,cpp}` for shared
project-error formatting, and loader fixtures/tests.
Read a manifest using native filesystem paths, parse TOML through the adapter,
and translate I/O and parse failures into located project diagnostics. Keep an
in-memory parse entry for tests and return an owned private parsed document.
Apply a documented manifest byte-size limit before parsing, with a smaller
injectable limit for boundary tests; retain the TOML library's nesting checks.

Acceptance: missing/unreadable file, directory input, malformed TOML, duplicate
TOML keys, Unicode/space-containing paths, and correct error line/column. Test
size-limit boundaries and deterministic I/O failures through an injectable reader
when permissions differ.
Run the common gate.
Commit: `feat(project): load TOML manifests with located errors`.

### Step 5 — Decode project and source declarations

Scope: `project/decode.{hpp,cpp}` and schema tests.
Decode version, target names, source arrays, source directories, and output into
owned records. Validate required fields, types, unknown keys, uniqueness, and
target nonemptiness. Reserve only the documented `options` table for step 6;
keep the decoder internal until complete rather than exposing partial CLI support.
Define documented target-count and total configuration-entry limits, shared with
the options decoder, and reject exhaustion without a partial successful result.

Acceptance: single/multiple targets in declaration order; absent/unsupported
versions, empty target lists, duplicate/invalid names, malformed arrays, empty
strings, unknown root/target keys, and count-limit boundaries. Run the common gate.
Commit: `feat(project): decode target and source declarations`.

### Step 6 — Decode typed frontend options

Scope: `project/decode_options.{hpp,cpp}`, its decoder call site, and option tests.
Decode include/search directories, definitions, application mappings, and feature
lists. Preserve order and locations, reject unknown nested keys and conflicting
feature lists, and retain macro literal strings for existing frontend validation.

Acceptance: all example values, TOML quoting, wrong scalar/array/map types,
unknown options, empty map values, conflicting feature entries, and records that
survive parsed-document destruction. Run the common gate.
Commit: `feat(project): decode per-target frontend options`.

### Step 7 — Resolve path bases and explicit sources

Scope: `project/paths.{hpp,cpp}` and path tests.
Implement manifest/invocation base handling and literal-file search fallback.
Resolve option paths without modifying cwd. Keep explicit source validation and
filesystem identity helpers separate from lexical normalization.

Acceptance: invocation from another cwd, absolute/relative/parent paths, project
symlink base, ordered source-search fallback, existing-invalid candidates, spaces,
Unicode, and literal files outside the project. Run the common gate.
Commit: `feat(project): resolve manifest paths and source search roots`.

### Step 8 — Implement pure wildcard matching

Scope: `project/glob.{hpp,cpp}` and matcher unit tests.
Parse pattern components and implement the specified `*`, `?`, and `**` grammar
without shell expansion or platform glob libraries. Use bounded iterative matching
with explicit work limits; separate UTF-8/component matching from path matching.

Acceptance: zero/multiple `**` levels, separator boundaries, Unicode `?`, hidden
names, case differences, unsupported constructs, invalid encoding, and adversarial
repeated-wildcard inputs with a deterministic limit diagnostic. Run the common gate.
Commit: `feat(project): add portable bounded wildcard matching`.

### Step 9 — Discover wildcard and directory sources

Scope: `project/discovery.{hpp,cpp}` and temporary-tree tests.
Traverse roots, apply the matcher, filter `.erl`, implement symlink policy, and
return sorted expansions. Set documented finite entry/depth/work budgets in an
internal limits object and allow small injected limits in tests. Report exhausted
limits and I/O failures without returning a silently partial successful result.

Acceptance: recursive/direct globs, recursive source directories, unmatched
patterns, empty directories, extension filtering, traversal errors, directory
symlink cycles, regular-file links, and deterministic order across file creation
orders. Run the common gate.
Commit: `feat(project): discover ordered sources from patterns and directories`.

### Step 10 — Assemble each target's unique source list

Scope: `project/sources.{hpp,cpp}` and assembly tests.
Combine literal lookup and expansions in the contracted order. Deduplicate by
filesystem identity within a target while retaining diagnostic spelling; avoid
an unconditional quadratic pairwise identity scan for ordinary files. Reject a
target with no resulting sources.

Acceptance: overlapping patterns, directory/file overlap, `..` aliases, symlink
and supported hard-link/case aliases, and independent inclusion in two targets.
Verify search paths do not implicitly add files or resolve wildcard roots.
Run the common gate.
Commit: `feat(project): assemble deterministic unique target source lists`.

### Step 11 — Select targets independently of filesystem access

Scope: `project/selection.{hpp,cpp}` and selection tests.
Implement default-all and explicit ordered selection, duplicate-selector removal,
and available-name diagnostics. Take the already decoded project and selector
strings; perform no source discovery or frontend work here.

Acceptance: one/all/subsets, reversed selection order, repeated selectors,
case-sensitive unknown names, and unselected absent trees. Run the common gate.
Commit: `feat(project): select named targets in requested order`.

### Step 12 — Compose effective target frontend options

Scope: `project/options.{hpp,cpp}` and composition tests.
Translate typed configuration into `PreprocessorOptions` and apply CLI overrides
according to the precedence table. Normalize CLI paths before composition; keep
an immutable effective settings value per target and copies per frontend session.

Acceptance: CLI include ordering over manifest directories, application replacement,
feature precedence, macro literal preservation and duplicate-definition errors
through existing preprocessing, and no cross-target option mutation. Run the common gate.
Commit: `feat(project): compose target and command-line frontend settings`.

### Step 13 — Construct a validated invocation plan

Scope: `project/plan.{hpp,cpp}` and plan tests.
Connect selection, selected-target source assembly, option composition, and output
resolution. Represent ordered target/file work and future destinations explicitly.
Enforce single-target CLI output overrides and compilation-output collision checks.
Return no executable plan when project/discovery validation fails.

Acceptance: invalid unselected schema still fails; absent unselected sources do
not; selected source failure prevents all execution; output defaults, relative
CLI output, normalized/alias collisions, and check-mode output ignoring. Assert
planning creates no files or directories. Run the common gate.
Commit: `feat(project): prepare validated target invocation plans`.

### Step 14 — Extract a reusable per-file frontend request

Scope: `driver/frontend.{hpp,cpp}`, existing driver declarations/call sites, and
focused frontend/CLI regression tests.
Separate frontend mode/settings from CLI input selection. Expose one per-file
processing operation with diagnostic context supplied by the caller. Keep existing
positional iteration and stdout/stderr behavior intact; do not add project CLI yet.
Keep this shared functionality in the driver and expose it through a small
request/callback adapter usable by the project executor without a library cycle.

Acceptance: all existing CLI tests, combined check/print modes, recovery, encoding
errors, warning-only success, and per-file macro isolation. Run the common gate.
Commit: `refactor(driver): extract isolated per-file frontend execution`.

### Step 15 — Execute prepared project targets

Scope: `project/execution.{hpp,cpp}`, project diagnostic context formatting in
`project/diagnostics.{hpp,cpp}`, and project execution tests using prepared plans.
Iterate target/file requests through the extracted frontend operation, label
diagnostics with project/target context, and aggregate failures without skipping
later valid files. Use the existing unsupported-compilation diagnostic for backend
requests. Accept the per-file executor and diagnostic sink at the boundary;
keep target loops and failure aggregation inside `project/`. Do not add CLI syntax
in this step.

Acceptance: same file under different target definitions, later targets after a
frontend failure, unambiguous diagnostic context, deterministic print ordering,
and no output/directory writes for checks or unsupported compilation.
Run the common gate.
Commit: `feat(project): execute targets with isolated frontend sessions`.

### Step 16 — Expose project and target CLI options

Scope: `project/cli.{hpp,cpp}`, `project/command.{hpp,cpp}`, thin hooks in
`driver/options.{hpp,cpp}` and `main.cpp`, and `tests/compiler/project/cli.cmake`.
Own project/selector fields, operand parsing, combination validation, and project
help text in `project/cli`. Track generic explicit-output presence in the driver
and pass it alongside other shared invocation settings. Connect the complete
loader/planner/executor in `project/command`; existing CLI code only delegates
project arguments, validation, help, and dispatch. Preserve positional behavior.

Acceptance: project-invocation CLI rules above, including targets preceding
`--project`, repeated selectors, missing operands, `--`, help/version without project I/O, unknown names,
mixed inputs, output conflicts, and success through each frontend mode. Register
separate project CLI CTest coverage and run the common gate.
Commit: `feat(cli): accept project manifests and repeated target selectors`.

### Step 17 — Render the annotated default project

Scope: `project/template.{hpp,cpp}` and template tests.
Implement pure rendering of the canonical starter, with one `app` target,
explicit default settings, platform-default output, and usage comments. Keep
template data independent of filename spelling and source-tree contents. Use
the existing loader/decoder in tests to prevent template/schema drift.

Acceptance: decode generated text, assert exactly one target and every default
field, preserve required annotations, and compare deterministic UTF-8/LF output
for both platform suffix variants. No filesystem operations. Run the common gate.
Commit: `feat(project): render annotated default manifest template`.

### Step 18 — Create a new manifest without overwriting files

Scope: `project/create.{hpp,cpp}` and creation tests.
Implement filename validation, extension completion, and exclusive writing of
the rendered template. Keep filename normalization separate from I/O. Handle
creation, write, and close errors with destination diagnostics and cleanup of
this invocation's incomplete file where possible. Do not create parent directories.

Acceptance: bare names, existing .toml/.TOML suffixes, other extensions, invalid
filename components, absolute/relative/Unicode/space-containing paths, missing
parents, existing files/directories/symlinks, and simultaneous creators. Use
injected I/O failures to test partial writes and close errors deterministically;
verify existing content remains unchanged. Run the common gate.
Commit: `feat(project): create starter manifests without overwriting destinations`.

### Step 19 — Expose the project creation CLI command

Scope: `project/cli.{hpp,cpp}`, `project/command.{hpp,cpp}`, minimal generic option
presence/dispatch hooks if needed, and `tests/compiler/project/cli.cmake`.
Parse `--new-project <filename>` and store its request in `project/cli`, validate
conflicts using explicit frontend-option presence supplied by the generic driver,
and extend project-owned help. Dispatch creation in `project/command` after generic
help/version handling. Bypass project loading, target resolution, and frontend
execution for this command. Do not require any source input or put creation policy
in `main.cpp`.

Acceptance: missing/empty/repeated operands, every incompatible option category,
informational requests without writes, extension completion, success/failure
streams and exit codes, no extra filesystem artifacts, and no overwrite on a
second invocation. Generate a project in an empty directory, then add `src/main.erl`
and successfully run `--parse-check --project` on it. Run the common gate.
Commit: `feat(cli): add new-project manifest creation command`.

### Step 20 — Add project workflow regression fixtures

Scope: `tests/fixtures/project/` and project integration tests only.
Add a small two-target Erlang project with shared files, different definitions,
include/include_lib dependencies, and search/glob/directory inputs. Execute from
both the project directory and an unrelated working directory using native test
argument arrays, preserving semicolons and shell metacharacters in paths.

Acceptance: target subset/default-all behavior, same-source option isolation,
manifest diagnostics, selected/unselected missing trees, aggregate failures,
repeatable output, and sentinels proving existing outputs remain untouched.
Run the common gate.
Commit: `test(project): cover multi-target workflows and path edge cases`.

### Step 21 — Validate project portability and limits

Scope: project-specific portability/stress tests and `docs/project-validation.md`.
Exercise C++23/26 where supported, compiler-only/runtime-only configurations, and
ASan/UBSan where available. Cover manifest and discovery size/depth/work limits,
native Unicode paths, Windows separators/drive/UNC handling, and filesystem alias
behavior on Linux x86/ARM, Windows x86-family, and macOS Apple Silicon.
Include starter output suffixes, extension handling, native destination paths,
and exclusive creation behavior on each available host.
Exercise the limits introduced in the loader, decoder, matcher, and discovery
steps together; keep this step focused on validation and its recorded evidence.

Acceptance: record exact host/toolchain commands and outcomes, conditionally
identify unavailable filesystem capabilities, and mark unavailable hosts pending.
Do not claim cross-platform validation from one host. Run the common gate.
Commit: `test(project): record portability and resource-limit validation`.

### Step 22 — Publish working project usage

Scope: `README.md`, `docs/projects.md`, a minimal runnable project under
`examples/project/`, and `.agents/{arch,files}.md`.
Remove planned labels only for delivered behavior. Document dependency setup,
format, selection defaults, compile-option precedence, wildcard/search distinctions,
exit codes, frontend limitations, and actual portability evidence. Keep this plan's
status and `aimemory.md` aligned with completed steps.
Document `--new-project`, its annotated defaults and file-creation rules, and the
create/populate/check workflow alongside loading an existing project.

Acceptance: execute every documented frontend example, compare help to docs,
check relative links and TOML examples, and run the common gate.
Commit: `docs(project): publish manifest and target workflows`.

## Completion criteria

- `--new-project <filename>` creates one annotated default target in the expected
  `.toml` file, preserving existing destinations and requiring no source tree.
  Generated manifests decode successfully and can be checked after adding sources.
- Project and positional workflows obey the documented selection, path, option,
  failure, and output contracts through all existing frontend modes.
- Named targets can independently process shared source files with distinct
  configuration; discovery and printed output are deterministic on each host.
- Project implementation, private headers, command handling, diagnostics, template
  resources, and local build/dependency wiring live under `compiler/src/project/`.
  Generic driver changes remain thin integration hooks plus the shared frontend
  extraction; tests, docs, and examples retain conventional repository locations.
- Every implementation step has its own passing-checks commit. The final full
  compiler/runtime build, CTest suite, formatting, Lizard, and clang-tidy pass.
- Runtime-only builds remain independent of project/frontend dependencies.
  Pending host evidence is visible; unsupported backend work is never reported
  as successful compilation. Stage-reader scope remains unchanged.
