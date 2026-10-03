-module(pat_modifier_conflict).
-export([f/1]).
f(<<X/integer-float>>) -> X.
