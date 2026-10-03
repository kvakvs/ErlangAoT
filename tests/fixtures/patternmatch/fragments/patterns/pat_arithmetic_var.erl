-module(pat_arithmetic_var).
-export([f/1]).
f({A, A + 1}) -> A.
