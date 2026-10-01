%% Authored acceptance seed.
-module(invalid_arity).
-export([f/1]).
f(X) when is_integer(X, 1) -> 1.
