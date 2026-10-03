-module(pat_nested_call).
-export([f/1]).
f({length([])}) -> ok.
