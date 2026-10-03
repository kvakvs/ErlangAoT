# Pattern/guard step 3 validation — 2026-10-01

Implements **atoms and boolean values (F06)** from
[the plan](../.agents/11-plan.md#completed-patternmatch). [Runtime atoms](runtime-atoms.md)
defines storage, ownership, ABI revision 3 and failed-registration policy.
Steps 4–20 remain pending; no matching or guard execution is claimed.

| Dependency / provenance | Identity |
| --- | --- |
| Official reference | `maint-29`, fetched again on 2026-10-01; unchanged |
| Checkout / pin | `21776803ecd11f5fa948732c0ec66b8f325dedfc` |
| Installed oracle | OTP `29.1.1`, ERTS `17.1`, separate from the source checkout |
| Native runner | Windows x86_64, Visual Studio 18 x64 environment |
| Compiler / SDK | Clang/LLVM `23.1.2` |
| Quality tools | clang-tidy `22.1.8`, Lizard `1.24.0` |

The original untracked `references/otp/lib/stdlib/src/1.ir` is preserved. Pin,
grammar, corpus hashes and step-1 source identities are unchanged. Historical
validation records retain their original revisions and outcomes.

| Check | Result |
| --- | --- |
| Fresh combined Debug configure/build | Pass, compiler/runtime/testing enabled |
| Full CTest | **109/109 passed**, no failures/skips, 84.38 seconds |
| Atom CLI/OTP/native workflow | Pass at O0/O2 with specialization on/off; positional/project source modes; each native consumer runs twice |
| Spelling and boolean results | Seven results per run match OTP by UTF-8 spelling, including Unicode, empty and NUL atoms; true/false inspect as booleans |
| Table validation | 255 supplementary Unicode scalars accepted, 256 rejected; malformed, overlong, surrogate and out-of-range UTF-8 rejected |
| Ownership | Equal local spellings deduplicate across modules; independent runtimes reject foreign raw words, host arguments/results, copies, native/generated error payloads and generated handles; explicit spelling remap succeeds |
| Module transactions | Capacity failure, invalid spelling, duplicate descriptor address and duplicate modules reject without partial publication; missing remote bindings and failed registrations permit successful retry |
| Allocation faults | Every allocation ordinal through atom insertion and descriptor registration is swept until success; no partial indexes, module/slot publication or teardown leaks; retry deduplicates |
| Lifetime | Host/error atom spelling survives full runtime teardown; expired factories reject; generated bindings pin code image and immutable atom records |
| Existing workflows | Parser corpus/coverage, pinned suite parsing, stale-hash rejection, incorrect specs, failure transport, failed-batch nonpublication and runtime-only consumers pass |
| Foreign objects | All seven existing 32/64-bit ELF/Mach-O/COFF targets pass, now including atom-service references and revision-3 descriptors |
| Production quality | Full Lizard/clang-tidy pass over all **189** production translation units; original commands preserved, no threshold changes or suppressions |
| Additional harness checks | Native consumer and allocator fault tests pass focused clang-tidy; both C++ harnesses and the Python driver pass Lizard at threshold 10 |

The new `codegen_atoms` workflow checks the pinned `guard_SUITE.erl` source hash
before generating licensed adapters. `rb/3` supplies the literal `true`/`false`
return leaves; `csemi2/2` supplies `ok`. The adapters deliberately isolate those
literal returns into zero-arity functions. They do **not** test original guards,
patterns or clause selection. Authored helpers add identity, grouping, nested
remote calls, Unicode/empty/NUL atoms and a misleading integer result spec.
[Retained provenance](patternmatch-step3-evidence.json) records declarations,
adaptations, exact source/clause hashes and generated-source hashes. The live OTP
oracle compiles the same adapters and emits spelling bytes independently of IDs.

Registration interns module/export metadata and atom literals, then publishes one
immutable binding vector with its registry/image. Valid atoms retained after a
later failure still count against the configured cap. Intern insertion itself
rolls back both indexes. Global word identities are never recycled; failed
reservations may leave gaps. Per-runtime storage/limits stay independent.

All generated objects/consumers require rebuilding for descriptor revision 3.
The existing checked failure-channel services retain their revision-2 signatures.
Host Term gains an immutable atom pin; generated words/arrays and reserved heap
slots keep their target-width layout.

One full clang-tidy run ended unsuccessfully without a diagnostic for the unchanged
lexer translation unit. Its isolated rerun and the subsequent full, unchanged
189-unit gate passed. No file, check or compiler flag was excluded for that retry.

Linux x86/ARM, Apple Silicon and native Windows 32-bit execution remain unavailable.
Foreign inspection is not native execution. No new sanitizer result is claimed.
Concurrent access requires host serialization; worker synchronization, atom GC,
heap payload roots, source matching/guards and production startup remain deferred.

Reproduce with the documented native toolchain and local dependency overrides:

```text
cmake --preset debug --fresh -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON <toolchain/dependency overrides>
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build build/debug --target check-quality
```

The local gate and detailed logs are under ignored `build/patternmatch-step3/`;
all source fixtures, oracle/consumer drivers and test registration are tracked.
