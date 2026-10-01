%% Authored acceptance seed.
-module(invalid_syntax).
-export([f/1]).
f(X when is_integer(X) -> 1.
