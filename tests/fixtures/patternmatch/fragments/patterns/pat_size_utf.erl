-module(pat_size_utf).
-export([f/1]).
f(<<X:8/utf16>>) -> X.
