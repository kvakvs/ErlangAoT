-module(pat_bin_string).
-export([f/1]).
f(<<"abc", X/utf8>>) -> X.
