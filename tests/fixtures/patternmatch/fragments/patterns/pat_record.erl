-module(pat_record).
-export([f/1]).
-record(r, {x, y}).
f(#r{x = A, y = B}) -> {A, B}.
