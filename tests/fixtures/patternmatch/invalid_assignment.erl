%% Authored acceptance seed.
-module(invalid_assignment).
-export([f/1]).
f(X) when (X = 1) -> 1.
