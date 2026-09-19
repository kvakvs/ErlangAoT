# TOML projects

Status: planned. The commands and format below are not implemented yet.


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
# Executable generation is not implemented yet.
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

| Setting | Manifest meaning and CLI combination |
| --- | --- |
| `include_dirs` | Listed search order; prepend the existing CLI `-I` search vector, whose last supplied entry is first. Preserve the preprocessor's built-in include lookup around that vector. |
| `source_search_paths` | Listed fallback order for project source discovery only; no new positional-mode search option in v1. |
| `defines` | Append CLI definitions after manifest definitions; duplicate names, including across the boundary, retain the existing redefinition error. |
| `applications` | Manifest application map, then CLI application entries replace matching names. |
| Feature lists | Apply manifest enable/disable settings, then ordered CLI feature changes; CLI's final setting wins. |
| `output` | Per-target future destination, overridden by explicit CLI output for one selected target only. |

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


## Build dependency

Project support uses compiler-private toml++ 3.4.0 (MIT license, Mark Gillard).
The tested release is [upstream v3.4.0](https://github.com/marzer/tomlplusplus/releases/tag/v3.4.0).
Its source archive SHA-256 is
`8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155`.
Retain the upstream LICENSE file when distributing dependency sources.

On macOS, `brew install tomlplusplus` supplies the dependency when the formula
version is 3.4.0. CMake queries the installed formula prefix and reports the chosen
header directory, including for unlinked installations. An explicit TOML root
takes precedence; every selected installation is checked for the pinned version.

Install that version or extract its source into `build/deps/tomlplusplus-3.4.0`.
Alternatively pass `-DERLANG_AOT_TOML_ROOT=/path/to/tomlplusplus-3.4.0` when
configuring. No automatic downloads occur; runtime-only builds do not discover it.
The private build enables header-only parsing with exceptions and disables
unreleased TOML syntax. TOML headers never appear in public frontend interfaces.

## Configuration limits

The manifest byte limit is 1 MiB; the native reader enforces it while reading.
TOML nesting also retains the pinned parser's own checks. Internal tests can
supply smaller limits without changing production defaults.

Decoded manifests allow at most 1,024 targets and 100,000 total TOML nodes
(including tables, arrays, and scalar values). Exceeding either limit fails before
constructing a partial usable manifest.

Wildcard matching allows 1,000,000 state transitions per path match and uses
iterative matching with linear row storage; exhaustion is an explicit error.

Each source expansion allows 100,000 visited entries, 128 directory levels, and
16,000,000 total wildcard matching transitions across candidate files. These
are finite work bounds, not wall-clock deadlines for filesystem I/O.
