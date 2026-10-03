-module(pat_key_no_auto).
-export([f/1]).
-compile(no_auto_import).
f(#{length([]) := X}) -> X.
