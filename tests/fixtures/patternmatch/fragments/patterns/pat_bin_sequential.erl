-module(pat_bin_sequential).
-export([f/1]).
f(<<Sz:32, Tail:(4 * Sz - 4)/binary>>) -> Tail.
