-module(tail_calls).
-export([main/1, back/2]).

%% Long enough that a loop keeping one frame per iteration would need more
%% than 2^24 stack words (128 MiB on 64 bits): these frames take at least 11
%% words each, branch/2 frames 41.
-define(STEPS, 2000000).
-define(BRANCH_STEPS, 500000).

%% The first argument selects a loop of tail calls.
main(["local"]) ->
    show(count(id(?STEPS), 0));
main(["mutual"]) ->
    show(ping(id(?STEPS)));
main(["remote"]) ->
    show(tail_peer:bounce(id(?STEPS), 0));
main(["branches"]) ->
    show(branch(id(?BRANCH_STEPS), 0)).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% Self tail call carrying an accumulator.
count(0, Acc) -> {local, Acc};
count(N, Acc) -> count(N - 1, Acc + 1).

%% Mutual tail calls between two local functions.
ping(0) -> {mutual, ping};
ping(N) -> pong(N - 1).

pong(0) -> {mutual, pong};
pong(N) -> ping(N - 1).

%% Remote tail calls bouncing between two modules.
back(N, Acc) -> tail_peer:bounce(N, Acc).

%% Tail calls from case, if and begin/end clause bodies.
branch(0, Acc) ->
    {branches, Acc};
branch(N, Acc) ->
    case N rem 3 of
        0 ->
            branch(N - 1, Acc + 1);
        1 ->
            if
                Acc >= 0 -> branch(N - 1, Acc + 2)
            end;
        _ ->
            begin
                branch(N - 1, Acc)
            end
    end.
