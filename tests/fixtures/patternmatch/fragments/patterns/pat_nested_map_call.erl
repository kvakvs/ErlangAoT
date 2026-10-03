-module(pat_nested_map_call).
-export([f/1]).
f(#{a := {length([])}}) -> ok.
