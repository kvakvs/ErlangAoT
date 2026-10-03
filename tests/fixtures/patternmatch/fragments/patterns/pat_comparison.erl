-module(pat_comparison).
-export([f/1]).
f({1 =:= 1}) -> ok.
