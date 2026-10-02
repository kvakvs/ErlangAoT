-module(call_sibling).
-export([f/0]).
f() -> g(X = 1, X). g(A, _) -> A.
