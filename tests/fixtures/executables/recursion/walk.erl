-module(walk).
-export([pong/1, depth/1]).

%% The other half of a cross-module recursive pair.
pong(0) -> {done, pong};
pong(N) -> recursion:ping(N - 1).

%% Self recursion over a nested tuple.
depth(leaf) -> 0;
depth({node, Child}) -> 1 + depth(Child).
