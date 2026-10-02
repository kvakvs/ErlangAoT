-module(body_match).
-export([f/1]).
f(X) -> Y = X, Y.
