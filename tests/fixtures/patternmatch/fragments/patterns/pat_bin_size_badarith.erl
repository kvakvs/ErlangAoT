-module(pat_bin_size_badarith).
-export([f/1]).
f(<<X:(1 div 0)>>) -> X.
