-module(pat_key_no_auto_remote).
-export([f/1]).
-compile(no_auto_import).
f(#{erlang:length([]) := X}) -> X.
