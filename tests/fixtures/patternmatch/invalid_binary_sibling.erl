%% Authored acceptance seed.
-module(invalid_binary_sibling).
-export([f/1]).
f(<<X:N>> = <<N:8>>) -> X.
