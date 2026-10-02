-module(clause_leak).
-export([f/2]).
f(X, _) -> X; f(_, _) -> X.
