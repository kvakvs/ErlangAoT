-module(guard_leak).
-export([f/1]).
f(X) when Y = X -> Y.
