-module(bit_construct_unit_default).
-export([f/1]).
f(X) -> <<X/unit:2>>.
