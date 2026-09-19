-module(assign_enable).
-feature(compr_assign, enable).
f() -> [X || X = 1].
