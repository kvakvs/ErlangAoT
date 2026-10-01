%% Authored acceptance seed.
-module(invalid_wild_read).
-export([f/1]).
f(_) -> _.
