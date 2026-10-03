-module(pat_key_import_erlang).
-export([f/1]).
-compile(no_auto_import).
-import(erlang, [length/1]).
f(#{length([]) := X}) -> X.
