-module(pat_key_no_auto_other).
-export([f/1]).
-compile({no_auto_import, [length/1]}).
f(#{tuple_size({}) := X}) -> X.
