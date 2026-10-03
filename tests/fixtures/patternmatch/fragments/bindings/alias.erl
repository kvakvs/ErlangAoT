-module(alias).
-export([f/1]).
f(A = B) -> B.
