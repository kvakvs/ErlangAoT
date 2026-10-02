-module(sibling_local).
-export([f/0]).
f() -> {begin X = 1, X end, X = 2}, X.
