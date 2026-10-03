-module(pat_key_import).
-export([f/1]).
-compile({no_auto_import, [length/1]}).
-import(other, [length/1]).
f(#{length([]) := X}) -> X.
