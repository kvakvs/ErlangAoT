-module(tail_peer).
-export([bounce/2]).

%% Count one step and tail call back into the other module.
bounce(0, Acc) -> {remote, Acc};
bounce(N, Acc) -> tail_calls:back(N - 1, Acc + 1).
