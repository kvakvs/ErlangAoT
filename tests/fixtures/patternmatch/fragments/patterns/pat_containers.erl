-module(pat_containers).
-export([f/1]).
f({[], {}, [A, B | T], "abc", #{a := V}}) -> {A, B, T, V}.
