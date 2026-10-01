%% Authored acceptance seed.
-module(invalid_map_assoc).
-export([f/1]).
f(#{key => X}) -> X.
