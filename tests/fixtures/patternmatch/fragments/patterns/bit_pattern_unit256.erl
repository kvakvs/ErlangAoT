-module(bit_pattern_unit256).
-export([f/1]).
f(<<X:1/unit:256>>) -> X.
