%% Authored acceptance seed.
-module(invalid_call).
-export([f/1]).
f(X) when true orelse local(X) -> 1.
local(X) -> X.
