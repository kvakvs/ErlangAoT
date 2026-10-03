-module(bit_construct_unit256).
-export([f/1]).
f(X) -> <<X:1/unit:256>>.
