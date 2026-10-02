-module(guard_assign).
-export([f/1]).
f(X) when Y = X -> X.
