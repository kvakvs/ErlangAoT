-module(bit_construct_unit_high).
-export([f/1]).
f(X) -> <<X:1/unit:257>>.
