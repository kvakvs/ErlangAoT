-module(bit_pattern_utf_undefined).
-export([f/1]).
f(<<X:undefined/utf8>>) -> X.
