-module(pat_constants).
-export([f/1]).
f({1 + 2 * 3, -(4 div 2), bnot 1, 8 bsr 1, 5 rem 2, 1 bor 2, 3 band 1, 3 bxor 2, 1 / 2, $a}) -> ok.
