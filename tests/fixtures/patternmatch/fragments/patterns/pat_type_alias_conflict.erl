-module(pat_type_alias_conflict).
-export([f/1]).
f(<<X:1/bytes-bits>>) -> X.
