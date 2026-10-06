# ErlangAoT documentation

Short reference notes for the current implementation. Each file states behavior
and limits that exist today; plans and step history live in `.agents/`.

| Topic | Document |
| --- | --- |
| Preprocessing (`epp` behavior, includes, macros, features) | [preprocessor.md](preprocessor.md) |
| Parsing, AST ownership, syntax limits | [parser.md](parser.md) |
| TOML projects and target selection | [projects.md](projects.md) |
| Compiler CLI, artifacts, LLVM SDK, example | [compile.md](compile.md) |
| Executable entry, arguments, exit status, output streams, linking | [executables.md](executables.md) |
| Semantic analysis, types, inference, bindings | [semantic.md](semantic.md) |
| Type specialization policy | [specialization.md](specialization.md) |
| Generated-code ABI: terms, symbols, registration, failures, roots | [abi.md](abi.md) |
| Patterns, clauses and body matches | [patterns.md](patterns.md) |
| Guards and the guard BIF catalog | [guards.md](guards.md) |
| Term representations (atoms, numbers, containers, maps, bits, records) and printing | [terms.md](terms.md) |
| Native, qualified and anonymous records (OTP 29) | [native-records.md](native-records.md) |
| Runtime lifecycle, memory, standard output, code server, scheduler bookkeeping | [runtime.md](runtime.md) |
| Process heap contract: word layout, areas, admission, roots, collection | [runtime-heap.md](runtime-heap.md) |
| Execution model decision: frames, calls, tail calls, yield, exceptions | [execution-model.md](execution-model.md) |
| Deferred-feature (`notimpl`) reporting | [features.md](features.md) |
| OTP source reference pin and refresh procedure | [otp-reference.md](otp-reference.md) |
| Known behavior differences from Erlang/OTP | [differences.md](differences.md) |
| Validation baseline, test modes, platform gaps, history | [validation.md](validation.md) |

Older per-step validation records and evidence JSON were condensed into
[validation history](validation.md#history); full originals remain in Git history
(last present at commit `2777c98`).
