# Patterns, clauses and body matches

Function clause heads, body matches, `case` clauses and `if` guards are executable for every
admitted term kind ([terms](terms.md)). Legality and availability are checked
separately: invalid Erlang is a **semantic** error even in unreachable code;
legal code that needs a missing feature gets a **capability** diagnostic; a
runtime **mismatch** is never a compiler error.

| Context | Status |
| --- | --- |
| Function heads + guards | Implemented; exhaustion raises `error:function_clause` |
| Body matches and sequences, `begin`/`end` | Implemented; failure raises `error:{badmatch, RHS}` |
| `case` clauses + guards | Implemented; exhaustion raises `error:{case_clause, Value}` |
| `if` guard clauses | Implemented; exhaustion raises `error:if_clause` |
| `maybe`, comprehensions | Capability (backlog F16) |
| `catch Expr` | Implemented; no patterns ([ABI](abi.md#failure-channel-revision-2)) |
| `try` `of` and catch clauses | Implemented; `of` exhaustion raises `error:{try_clause, Value}`, unmatched exceptions re-raise and `after` runs on every path ([ABI](abi.md#failure-channel-revision-2)); `Class:Reason:Stack` binds the [stack trace](abi.md#stack-traces) |
| Fun clauses | Capability (F18) |
| `receive` | Capability (F22/F25) |

## Pattern forms

| Form | Rule | Rejection |
| --- | --- | --- |
| `_` | Never binds; occurrences independent | Reading `_` is semantic |
| `Name`, `_Name`, repeats | Single assignment; repeats need exact equality (`1` ≠ `1.0`) | Unbound/unsafe read semantic; inequality mismatches |
| `P1 = P2`, parentheses | Both constrain the same value; no key/size bindings between siblings | Illegal sibling dependency semantic |
| Atoms, integers, chars, floats | Exact equality; signed zero distinct | Mismatch |
| Constant arithmetic | OTP pattern operators folded exactly; `1 div 0` and non-constants rejected | Semantic |
| Tuples, lists, improper tails, strings | Exact arity and cons/nil shape; strings are lists | Mismatch |
| `"prefix" ++ Tail` | Literal string or literal integer list prefix only | Variable prefix semantic |
| `#{K := P}` | `:=` only; extra keys allowed; `#{}` tests type; keys are guard expressions over incoming bindings | `=>`, unbound key semantic; missing key mismatches |
| Bitstrings | Validated type/size/unit; earlier segments may size later ones; unsized tail last | Invalid specifier semantic; short data mismatches |
| Tuple records `#r{f = P}`, `#r.f` | Expanded to tuple constraints; omitted fields unconstrained | Unknown record/field semantic |
| Native/anonymous records | Not implemented | Capability (F17) |
| Calls, variable arithmetic, other expressions | Not patterns | Semantic |

## Scopes

Map keys and binary sizes read the incoming environment, never siblings:
`#{K := V} = #{key := K}` and `<<X:N>> = <<N:8>>` cannot bind their own key or
size. Inside one binary, a size may read earlier segments: `<<N:8, X:N>>` is
legal; `{N, <<X:N>>}` needs `N` bound beforehand. Constant bad arithmetic in a
key or size is legal and fails at match time.

Binary validation rejects nested-container/alias segments, conflicting modifiers,
bad unit ranges, unit without size for integer/float defaults, invalid UTF
size/unit, typed/sized literal strings and non-final unsized binary segments.

## Execution

- A flat match plan per head/LHS over original argument slots; aliases share an
  input, first definitions bind SSA values, repeats emit exact-equality tests.
  Shape checks dominate extraction; extraction uses checked runtime services.
- Clauses run in source order, each with a fresh environment that reloads the
  original arguments. Head mismatch or guard rejection goes to the next clause;
  body errors and infrastructure failures never retry later clauses.
- A `case` evaluates its scrutinee once, then tries each clause in order with
  the scrutinee as the single plan input: pattern mismatch or guard rejection
  (including guard errors) goes to the next clause, exhaustion raises
  `{case_clause, Value}`. Every clause starts from the bindings before the
  case; the case value and each exported binding join in one PHI per value.
- An `if` is a `case` without scrutinee or patterns: each clause guard is tried
  in order (a guard error rejects the clause), exhaustion raises `if_clause`,
  and values and exports join the same way.
- `begin`/`end` runs its sequence in the enclosing scope and yields its last
  value.
- Body sequences run in order and return the last value. A match evaluates its
  RHS once, binds new names, checks existing ones and returns the RHS (also for
  `_ = RHS`). Chains evaluate the innermost RHS first.
- Atom literals load module bindings; matching never interns atoms.
- Unconditional variable heads compile to compact projection IR.

## Limits

Pattern normalization shares the module's 1,000,000-unit semantic budget;
constants are capped at 10,000 decimal digits and shifts at 1,000,000 bits.
Each match plan has a 100,000-work ceiling. The parser separately limits nesting
to 256 (hard ceiling 512).
