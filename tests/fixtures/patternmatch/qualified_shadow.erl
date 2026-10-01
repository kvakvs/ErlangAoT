%% Authored acceptance seed.
-module(qualified_shadow).
-export([f/1]).
-compile({no_auto_import,[is_integer/1]}).
f(X) when erlang:is_integer(X) -> 1.
is_integer(X) -> X.
