-module(pat_aliases).
-export([f/1]).
f(({A, B} = {B, A}) = Whole) -> {Whole, A, B}.
