-module(compound).
-export([f/1]).
f({X, X, _, _Name}) -> {_Name, X}.
