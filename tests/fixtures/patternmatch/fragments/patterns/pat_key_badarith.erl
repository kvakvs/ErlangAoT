-module(pat_key_badarith).
-export([f/1]).
f(#{8 div 0 := Selected}) -> Selected.
