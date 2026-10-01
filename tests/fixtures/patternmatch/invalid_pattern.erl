%% Authored acceptance seed.
-module(invalid_pattern).
-export([f/1]).
f(X) -> abs(1) = X.
