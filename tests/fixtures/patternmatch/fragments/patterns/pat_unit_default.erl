-module(pat_unit_default).
-export([f/1]).
f(<<X/unit:8>>) -> X.
