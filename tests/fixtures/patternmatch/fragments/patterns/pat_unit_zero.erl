-module(pat_unit_zero).
-export([f/1]).
f(<<X:8/unit:0>>) -> X.
