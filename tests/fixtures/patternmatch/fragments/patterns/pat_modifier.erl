-module(pat_modifier).
-export([f/1]).
f(<<X/banana>>) -> X.
