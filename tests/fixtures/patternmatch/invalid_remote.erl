%% Authored acceptance seed.
-module(invalid_remote).
-export([f/1]).
f(X) when lists:member(X, []) -> 1.
