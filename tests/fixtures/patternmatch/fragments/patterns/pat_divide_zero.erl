-module(pat_divide_zero).
-export([f/1]).
f({1 div 0}) -> ok.
