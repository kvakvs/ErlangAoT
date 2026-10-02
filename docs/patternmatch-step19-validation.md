# Pattern matching step 19 validation

Completed 2026-10-03 on Windows x64. The official maint-29 fetch matched the clean
checkout/pin `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Oracle OTP 29.1.1 /
ERTS 17.1; LLVM SDK 23.1.2, Lizard 1.24.0, clang-tidy 22.1.8.

Fresh Debug compiler/runtime/testing ON, OTP audits OFF, deliberately absent OTP
paths: build passed, **123/123 CTests**, zero skips, 121.19 s. Lizard passed
unchanged CCN 10; clang-tidy passed all **258 production units**. Formatting and
whitespace checks pass. Logs: build/patternmatch-step19/{gate,tests,quality}.log.

[The facts contract](binding-facts.md) records indexed whole-value body facts,
conservative extracted values, isolated candidate SSA and common-relation joins.
The owned corpus retains 822 OTP outcomes for paired annotated/unannotated kernels,
including a complete renamed match_SUITE helper and explicit authored extensions.
All four native policies and both CLI drivers pass local/remote workflows.

Public type inspection checks constant propagation, whole-argument relations and
loss of relations at extraction/joins. Both IR modes run in all four policies for
32/64-bit targets, with LLVM verification before/after optimization. CFG dominator
checks pass 712 service-output observations, 192 shape-before-extraction observations
and 72 checked-value/bit-cursor observations. Generated source has no unchecked
integer-to-heap-pointer conversion. [Evidence](patternmatch-step19-evidence.json)
retains the concrete function/block records.

Private budget tests now use real scalar/container/map/bitstring/record/float source.
A tiny inference ceiling still verifies, optimizes and emits through generic
fallback. Module/batch byte and inspection ceilings erase all staged outputs.
Existing variant/profile-work/measured-growth caps and whole-draft rollback pass
without threshold changes or new unsupported check removal. Focused tests and
production tidy passed before the fresh full gate. Native execution on Linux,
Apple Silicon and 32-bit hosts and a new sanitizer run remain unavailable.
