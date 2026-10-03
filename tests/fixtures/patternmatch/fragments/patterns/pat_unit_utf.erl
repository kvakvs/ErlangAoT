-module(pat_unit_utf).
-export([f/1]).
f(<<X/utf8-unit:8>>) -> X.
