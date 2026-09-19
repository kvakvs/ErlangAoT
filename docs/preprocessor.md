# Preprocessor behavior and validation

The language baseline is OTP 29.1, source commit
`751f87b703fe5948607d08e82599ce644b772e76`. Production preprocessing is native C++;
Installed OTP 29+ is the test oracle, validated by CMake for native compiler test
builds (see README for discovery/overrides). Boost.Parser handles character rules; shared compiler
metadata supplies operator precedence. An explicit cursor parses project tokens because the pinned parser's
public input interface requires character code units. Expanded tokens are never
converted to source text to fit that interface.

## Coverage

| Plan step | Implemented behavior | Evidence |
| --- | --- | --- |
| 4 | Source-order object definitions, initial terms, redefinition/undefinition, dependency cycles | macros, recursion, rescan, API/CLI tests |
| 5 | Object/arity overloads, raw substitution, nested block/fun arguments, expansion budgets | arguments, macros, generated API cases |
| 6 | Raw argument stringification using canonical OTP token values | macros exact string tokens |
| 7 | Conditional nesting, skipped effects/errors, ordering and EOF diagnostics | conditions, errors, API tests |
| 8 | Source directory/cwd/include search, environment substitution, application fallback, guarded recursion | includes, virtual filesystem tests, CLI collisions |
| 9 | Module/base module, file/line mapping, expanded function headers, compatibility macros | context, rescan, location API tests |
| 10 | Guard validation, precedence, arbitrary integers, Erlang values/operators/BIFs | guards matrix, conditions, resource tests |
| 11 | OTP feature defaults, prefix rules, keyword changes, query macros | features, arguments, CLI feature options |
| 12 | Literal term diagnostics, warning-only success, error latching and provenance | terms, errors, include/location tests |
| 13 | Preprocessing CLI, offline/live reference comparisons, OTP header smoke tests | CLI, golden/oracle suites |

`PreprocessorSession` returns expanded ordinary forms (including implicit `-file`
attributes) and structured diagnostics. Its buffers outlive the session through
shared token/span ownership. `DirectiveReader` retains the earlier syntax-only API.
This is an internal compiler interface, not a chosen public intermediate format.
Expanded forms now also retain immutable enabled-feature snapshots, and `features()`
exposes current/final session state for the parser. Syntax-only forms leave it unspecified.

Parser integration corrected the return-line value in implicit `-file` attributes:
an immediate LF after an include's terminating dot advances the returned line, while
space, comment, and CRLF endings follow OTP's one-character `scan_dot` behavior.
`tests/fixtures/parser/phase1.erl` compares these attributes directly against OTP;
ordinary token locations, spelling, and existing preprocessor records are unchanged.

## Compatibility policies

- Include lookup uses the physical current source directory, configured working
  directory, then include paths in supplied API order. The CLI reverses repeated
  `-I` options to match Erlang command-line precedence. Absolute host paths bypass
  this search. `include_lib` first tries ordinary paths, then the explicitly supplied
  application directory. No Erlang code server or version-directory guessing is used.
- A leading `$VARIABLE` path component uses the injected environment resolver.
  An unset variable retains its literal spelling. Filenames must be literal strings;
  epp does not macro-expand include operands. Files decode independently as UTF-8 or
  declared Latin-1. Host filesystem path conventions apply, independently of target CPU.
- Includes share macros/features/module-prefix state; each file owns its conditional
  stack and logical file mapping. Include guards can terminate recursion; an active
  path is not rejected merely because it has been encountered before.
- `OTP_RELEASE` is 29 and `MACHINE` is `'BEAM'` for source compatibility. Neither
  describes native output or the host CPU. `self()` is a stable synthetic preprocessing
  pid and `node()` is `nonode@nohost`; conditions never execute Erlang runtime code.
- Conditions validate all branches before short-circuit evaluation. Invalid guard
  syntax is an error; unbound variables, non-boolean results and ordinary evaluation
  exceptions select false. Records are not expanded here: record field/index evaluation
  fails, while an unselected short-circuit record access is valid.
- OTP 29.1's epp/erl_lint crashes on record construction in a condition. This compiler
  emits a controlled diagnostic (`record construction requires record expansion`).
  Regression coverage is native rather than an oracle fixture that kills epp.
- Initial values are parsed as Erlang literal terms and normalized into tokens, including
  signed numeric tokens and Unicode integer lists. Binary/external-fun initial terms
  are accepted here; OTP 29.1's `erl_parse:tokens/1` lacks those abstract-term clauses.
- Feature data is fixed to OTP 29.1: `maybe_expr` is approved, enabled by default,
  and controls `maybe`/`else` keywords; `compr_assign` is experimental and disabled
  by default. These are the release's only configurable features. There are no
  permanent or rejected entries to enable; other names are errors. Initial `all`
  selects both; a source `-feature(all, ...)` is invalid, as in epp.
  Feature support here does not imply backend support.
- Expression values retain arbitrary-size integers, exact map keys, improper lists,
  tuples, Unicode strings, binaries/bitstrings and external-fun diagnostic terms.
  Guard calls use the closed `erl_internal:guard_bif/2` and type-test catalogs, plus
  qualified Erlang arithmetic/boolean/comparison operators. No arbitrary calls occur.

## Limits and diagnostics

Default per-form expansion limits are 256 dependency/expansion levels and 1,000,000
produced tokens; include depth is 64 and expression nesting is 256. API clients can
lower or raise these counts through `PreprocessorLimits`. Integer and bitstring values
have an additional fixed 1,000,000-bit allocation bound. Budget exhaustion is a
resource diagnostic, not a silently false condition. These are implementation limits.

Diagnostics carry stable categories, severity, owned physical spans, optional logical
coordinates, and related macro/include spans. Warnings continue without failure;
errors latch failure while recovery proceeds at a lexical form boundary. The next
compiler stage remains responsible for ordinary Erlang syntax/semantic validation,
records, parse transforms, and code generation. External documentation-file ingestion
is outside the preprocessor chapter implemented here.

## Reference tests

`semantic/*.erl.tokens` records epp token kinds, decoded values and diagnostic-event
order. Floats preserve IEEE binary64 bits. Only fixture-root paths are normalized;
implicit file attributes are deliberately omitted from this comparison and tested
through the native API. Important logical locations and provenance are asserted
separately. Diagnostic wording is not required to equal OTP's text.

`semantic/headers/otp_assert.hrl` and `otp_file.hrl` are copies from the pinned
OTP release with trailing whitespace normalized and their upstream Apache-2.0
notices preserved. Their source
paths are `lib/stdlib/include/assert.hrl` and `lib/kernel/include/file.hrl`. They are
used by `otp_headers.erl`; all other semantic fixtures were authored for this project.
The full upstream Common Test suites have not been run.

On macOS arm64, run the full debug suite, the selectable C++26 build, ASan/UBSan,
and `check-quality`. Linux x86/ARM and Windows x86-family execution remains
outstanding; no cross-host success is implied by native filesystem usage.
