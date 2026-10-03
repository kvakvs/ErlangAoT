-module(pat_key_no_auto_one).
-export([f/1]).
-compile({no_auto_import, [length/1]}).
f(#{length([]) := X}) -> X.
