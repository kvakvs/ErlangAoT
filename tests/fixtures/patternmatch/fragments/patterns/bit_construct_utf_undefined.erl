-module(bit_construct_utf_undefined).
-export([f/1]).
f(X) -> <<X:undefined/utf8>>.
