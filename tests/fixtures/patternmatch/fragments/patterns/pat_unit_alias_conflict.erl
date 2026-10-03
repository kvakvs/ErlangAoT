-module(pat_unit_alias_conflict).
-export([f/1]).
f(<<X:1/bytes-unit:1>>) -> X.
