# Type specialization

Speed mode (`-O2`) may clone a function into representation variants guarded by
runtime tag checks. `-O0` and `--no-type-specialization` produce generic code only.

## Policy

- Profiles come from inferred call-site facts. Today only exact,
  target-representable integer singletons prove a small-integer argument.
  Specs, broad `integer()`, unions and unknowns stay generic.
- A variant is justified only if it removes an implemented check: an exact
  low-tag comparison on an argument load in the entry block, before side effects.
- Limits: 3 variants per function, 32 per module, 128 per target. Estimated and
  then actual growth (clone + guards + dispatch + fallback) must stay within 2x
  the generic instruction count per function and module.
- Generic bodies are always kept. Over-budget drafts are discarded silently;
  the program never fails because of specialization.
- Selection is deterministic (module, symbol, profile order).

## Lowering

LLVM cloning/simplification replaces proven checks inside the clone. A bounded
dispatcher tests every constrained tag and forwards context and arguments to a
variant or the generic body; the public symbol and ABI do not change. No
unchecked unboxing, spec assumptions or fast-math flags are introduced.

## Current effect

The current source subset has no removable representation checks, so real
Erlang source receives zero variants even at O2; specializing an identity would
only grow code. Focused LLVM fixtures inject repeated tag checks to exercise
hits, fallbacks and growth rollback with native execution.

`codegen_measurements` writes `measurements/<config>/measurements.json` in the
codegen test build directory (compile time, IR/object bytes, variants, native
time). Times are descriptive, never thresholds. Windows x64 / LLVM 23.1.2 sample
(2026-09-29): source workload 232,876 IR / 40,498 object bytes at O0 and
204,408 / 35,886 at both O2 policies, zero variants; the synthetic fixture added
30 instructions to a 55-instruction function for one accepted variant.
