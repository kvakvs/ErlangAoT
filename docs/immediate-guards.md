# Immediate guard services

Pattern/guard step 7 separates source legality from executable service availability.
A bounded semantic walk checks every guard operand before optimization, including
unreachable operands. Illegal calls, arities, assignments and dynamic targets keep
their source locations. Legal unavailable signatures produce a capability diagnostic.
Runtime registration cannot authorize or replace a guard call.

Explicit `erlang` calls use the pinned guard/operator catalog. Unqualified guard
BIFs honor local shadowing, explicit imports and all/selective `no_auto_import`;
only these inert compile options and admitted `erlang` imports become executable
metadata. Legacy tests such as `integer(X)` are recognized only at guard-test roots
(including parentheses). `float(X)` there is a predicate; nested/qualified `float/1`
is the deferred conversion. Record-specific arguments/layouts remain step 17.
Semantic/budget failure clears all service-resolution tables before reuse.

The runtime admits small integers, owned atoms, `[]` and `{}`. Executable signatures:

| Family | Operations | Present-domain behavior |
|---|---|---|
| Predicates | `is_atom`, `is_integer`, `is_number`, `is_boolean`, `is_tuple`, `is_list`, `is_binary`, `is_bitstring`, `is_float`, `is_map`, `is_pid`, `is_port`, `is_reference`, `is_function` at arity 1 | Classify real admitted values; absent representations return false |
| Function arity | `is_function/2` | Nonnegative integer arity returns false; other arities raise/reject `badarg` |
| Comparisons | `=:=`, `=/=`, `==`, `/=`, `<`, `=<`, `>`, `>=`, including qualified operator calls | Exact equality shares the matcher; loose and exact equality coincide on this domain |
| Queries | `tuple_size/1`, `length/1`, `size/1`, `element/2`, `hd/1`, `tl/1` | Empty tuple/list sizes are zero where OTP permits; wrong types and empty extraction are `badarg` |
| Selection | `min/2`, `max/2` | Return the original selected term |

Ordering compares decoded integers, then atoms by validated UTF-8 spelling, then
empty tuples, then nil. UTF-8 byte order preserves Unicode scalar order. Atom slot
or allocation order never defines term order. Boxed numbers, lists and containers
will extend the shared services in their representation steps.

`erlang_aot_immediate_v1` returns success, semantic bad argument, or infrastructure
failure; it writes its output only on success. Generated code checks the context
failure channel before reading output. A reached semantic `badarg` rejects the
enclosing guard; in a body it raises Erlang `error:badarg`. Resource, allocation,
ownership and internal failures preserve their exact infrastructure status and
cannot select a successful result. Canonical boolean slots are initialized during
module registration; expression evaluation never interns them.

Single-test guards run after a successful immediate head and accept canonical
`true` only. Comma/semicolon grouping and boolean operators belong to step 8;
ordered function clauses remain step 9. Ordinary bodies use the same predicates,
comparisons and queries. Specs never authorize unchecked representations; inference
and specialization retain conservative facts for service results.
