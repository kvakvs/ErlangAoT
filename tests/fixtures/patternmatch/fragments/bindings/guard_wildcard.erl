-module(guard_wildcard).
-export([f/1]).
f(X) when _ =:= X -> X.
