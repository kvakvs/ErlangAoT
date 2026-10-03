-module(pat_body_compound).
-export([f/1]).
f(V) ->
    ({A, B} = {B, A}) = V,
    {A, B}.
