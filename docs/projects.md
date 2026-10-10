# TOML projects

A project manifest selects ordered groups of Erlang sources and configures each
target independently. All CLI actions accept projects. See the
[working example](../examples/project/project.toml).

## Manifest (schema 1)

```toml
schema_version = 1

[[targets]]
name = "app"
sources = ["main.erl", "src/workers/*.erl", "shared/**/*.erl"]
source_dirs = ["src/support"]
output = "build/app"
entry = "main:main"

[targets.options]
source_search_paths = ["src", "generated"]
include_dirs = ["include", "vendor/include"]
defines = ["DEBUG", "LIMIT=100", 'LABEL="demo"']
enable_features = ["maybe_expr"]
disable_features = ["compr_assign"]

[targets.options.applications]
my_dependency = "vendor/my_dependency"
```

- `schema_version = 1` and a nonempty `targets` array are required. Unknown keys,
  wrong types, duplicate target names and empty strings are rejected; no type
  coercion. The whole manifest is validated, including unselected targets.
- Target names: case-sensitive ASCII `[A-Za-z0-9_][A-Za-z0-9_.-]*`, unique. A
  name is a build target, not an LLVM triple.
- `sources` (files and wildcard patterns) and `source_dirs` (recursive `.erl`
  discovery) are optional, but a target needs at least one entry. Explicit files
  must end in `.erl`; headers are dependencies, not inputs.
- `options` fields are optional typed frontend options. `defines` use
  `NAME` (= `true`) or `NAME=ERLANG_LITERAL_TERM`; duplicates are errors. A
  feature cannot appear in both feature lists.
- `entry` (optional) selects the executable entry `MODULE[:FUNCTION]`; see
  [executables](executables.md#entry-selection). CLI `--entry` overrides it for
  a single selected target.
- `output` is the executable destination, default
  `<manifest-dir>/build/<target>` (`.exe` on Windows). A target with `output` or
  `entry` is an executable target ([executables](#executables)); `--emit` and
  check/print modes ignore both keys' outputs.
- Not supported yet: root defaults, inheritance, target dependencies, imports,
  profiles, exclusions, packages, watch, caching, parallel builds. Fields are
  added only together with their behavior.

## CLI and target selection

```text
clau [options] <source.erl>...
clau [options] --project <path> [--target <name>]...
clau --new-project <filename>
```

- Positional sources and `--project` are mutually exclusive. One `--project`,
  any number of `--target`, in any order.
- No `--target`: all targets in manifest order. Selectors keep their order and
  first occurrence. Unknown names fail before any frontend work and list the
  available names.
- `--project test1` loads `test1.toml` when `test1` does not exist.
- CLI frontend options apply to every selected target. CLI-relative paths keep
  invocation-directory meaning; manifest paths are relative to the manifest.
- Without `--emit` or a check/print action, executable targets link to their
  outputs ([executables](#executables)); other targets compile in memory.
- `-o/--output` links the target to that path instead of its `output`
  ([linking](executables.md#linking)); it requires exactly one selected target.
- `--verbose` traces `[pp]` files/includes and `[parse]` sources on stderr.
- Exit 2: usage errors and unknown targets. Exit 1: manifest, discovery,
  frontend or creation failures. Exit 0: success (warnings allowed).

Each selected target is a separate compilation batch with its own preprocessing
sessions. Planning (decode, select, discover, check output collisions) finishes
before any source runs; execution then processes targets/files in order and
latches failures. Payload output goes to stdout in target/file order without
banners; context and diagnostics go to stderr. A target with diagnostics is
named once, on a `<manifest> [target <name>]:` line before its first one; the
diagnostics themselves (`error: file:line:col: message`) carry no tool or
target prefix.

```sh
./build/debug/bin/clau --parse-check --project examples/project/project.toml
./build/debug/bin/clau --print-ast --project examples/project/project.toml --target tests --target app
```

## Executables

`clau --project FILE [--target T]...` (no `--emit`, no check/print action)
links every selected target that has an `output` or `entry` key, or that CLI
`-o`/`--entry` addresses, into one executable per target:

- Destination: CLI `-o` (invocation-relative), else `output`
  (manifest-relative), else `<manifest-dir>/build/<target>`. Windows targets add
  `.exe` to a name without extension. Missing directories of manifest outputs
  are created; an explicit `-o` directory must exist.
- Entry: CLI `--entry`, manifest `entry`, else detection
  ([entry selection](executables.md#entry-selection)).
- Targets without `output`, `entry`, `-o` and `--entry` are libraries: they
  compile in memory and write nothing, even if a module exports `main/1`.
- Planning rejects selected targets with the same destination; outputs that
  coincide only after `.exe` is added are rejected before publication. An
  output must not alias a selected source or the manifest.
- Each target compiles and links into its own staging directory. Outputs are
  replaced only after every selected target succeeded, in target order; any
  failure keeps every existing output unchanged. A failure while replacing a
  later output can leave earlier ones already replaced.
- `--linker` and `--runtime-library` apply to every linked target
  ([linking](executables.md#linking)).

```sh
./build/debug/bin/clau --project tests/fixtures/linking/project/project.toml
```

## Creating a project

`--new-project <filename>` writes one annotated manifest and nothing else.

- Appends `.toml` unless the name already ends in `.toml` (ASCII
  case-insensitive): `demo` → `demo.toml`, `demo.config` → `demo.config.toml`.
- Rejects empty or directory-like names, repetition and any combination with
  sources, `--project`, `--target`, `-o`, check/print modes or frontend options.
- The parent must exist. Never overwrites (exclusive creation); cleans up its
  own partial file on write failure. Prints the created path on success.
- Content is deterministic UTF-8/LF with comments: one target `app`,
  `sources = []`, `source_dirs = ["src"]`, empty option lists and the platform
  default output. It decodes immediately; the user adds sources under `src`.

## Paths and discovery

- Project path resolves against the invocation directory; the manifest's
  lexical parent is the base for all manifest paths (even through a symlink).
  Absolute paths stay absolute; `..` is allowed. No shell, environment or tilde
  expansion. Use `/` or TOML literal strings for Windows paths.
- Literal relative sources try the base first, then `source_search_paths` in
  order. An existing but unreadable candidate is an error. Search paths only
  locate listed files; they do not enumerate modules or affect includes.
- Wildcards: `*` and `?` within a component, `**` as a whole component. Brackets,
  braces, negation and escapes are rejected. Matching is case-sensitive; `?` is
  one Unicode scalar of a valid UTF-8 name.
- Hidden entries are included; discovered directory symlinks are skipped
  (explicit roots may be symlinks); file symlinks are followed.
- Order: `sources` entries in order, each pattern's matches sorted by UTF-8 path
  bytes, then each `source_dirs` expansion sorted the same way. Missing literals,
  unmatched patterns and empty targets are errors.
- Deduplication within a target uses filesystem identity (symlinks, hard links,
  case aliases), keeping the first spelling. The same file in two targets is
  compiled twice because options can differ.
- Only selected targets touch the filesystem.

## Effective options

| Setting | Manifest and CLI combination |
| --- | --- |
| `include_dirs` | Manifest order, after CLI `-I` (last CLI `-I` searched first) |
| `source_search_paths` | Project source lookup only |
| `defines` | Manifest first, then CLI; duplicates across both are errors |
| `applications` | Manifest map; CLI entries replace matching names |
| Feature lists | Manifest settings, then ordered CLI changes; CLI wins |
| `output` | Per target, executable targets only; CLI `-o` overrides for a single selected target |
| `entry` | Per target; CLI `--entry` overrides for a single selected target |

## Build dependency

Compiler-private toml++ 3.4.0 (MIT), header-only with exceptions, unreleased
syntax disabled. Archive SHA-256
`8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155`.

- macOS: `brew install tomlplusplus` (formula 3.4.0) is detected.
- Windows: CMake downloads and verifies the release into `thirdparty/`.
- Elsewhere: install it, extract to `thirdparty/tomlplusplus-3.4.0`, or pass
  `-DCLAUSE_TOML_ROOT=<path>` (takes precedence, disables download).
- Runtime-only builds do not look for it.

## Limits

Manifest size 1 MiB; at most 1,024 targets and 100,000 TOML nodes. Each source
expansion allows 100,000 visited entries, 128 directory levels and 16,000,000
wildcard transitions. Exhaustion is an explicit error; these are work bounds,
not I/O deadlines.
