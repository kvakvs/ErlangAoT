%% Authored acceptance seed.
-module(invalid_map_sibling).
-export([f/1]).
f(#{K := V} = #{key := K}) -> V.
