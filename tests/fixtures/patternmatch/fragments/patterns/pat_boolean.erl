-module(pat_boolean).
-export([f/1]).
f({not true}) -> ok.
