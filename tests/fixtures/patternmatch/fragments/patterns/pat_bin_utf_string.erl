-module(pat_bin_utf_string).
-export([f/1]).
f(<<"abc"/utf16>>) -> ok.
