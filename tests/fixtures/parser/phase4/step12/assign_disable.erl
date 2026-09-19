-module(assign_disable).
-feature(compr_assign, disable).
f() -> [X || X = 1].
