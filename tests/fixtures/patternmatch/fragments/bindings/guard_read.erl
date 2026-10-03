-module(guard_read).
-export([f/1]).
f(X) when X =:= X -> X.
