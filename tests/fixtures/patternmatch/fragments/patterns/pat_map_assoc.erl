-module(pat_map_assoc).
-export([f/1]).
f(#{a => X}) -> X.
