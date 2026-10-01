%% Authored acceptance seed.
-module(invalid_shadow).
-export([f/1]).
-compile({no_auto_import,[is_integer/1]}).
f(X) when is_integer(X) -> 1.
is_integer(X) -> X.
