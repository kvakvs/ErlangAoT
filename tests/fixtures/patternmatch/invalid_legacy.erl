%% Authored acceptance seed.
-module(invalid_legacy).
-export([f/1]).
f(X) when true andalso integer(X) -> 1.
