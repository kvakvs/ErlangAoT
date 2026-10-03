%% Process ring: main -> P1 -> ... -> PN -> main. A token circles the ring
%% once per round, each process adding its index to the hop total.
-module(ring).
-export([main/1]).

%% Usage: ring SIZE ROUNDS.
main([SizeArg, RoundsArg]) ->
    Size = list_to_integer(SizeArg),
    Rounds = list_to_integer(RoundsArg),
    {First, Monitors} = spawn_ring(Size, self()),
    io:format("ring of ~b processes, ~b rounds~n", [Size, Rounds]),
    run_rounds(First, 1, Rounds),
    First ! stop,
    receive
        stop -> ok
    end,
    Seen = [collect(I) || I <- lists:seq(1, Size)],
    Expected = length(lists:filter(fun(N) -> N =:= Rounds end, Seen)),
    io:format("~b of ~b processes saw every token~n", [Expected, Size]),
    io:format("first counts: ~w~n", [take(5, Seen)]),
    Reasons = [await_down(Pid, Ref) || {Pid, Ref} <- Monitors],
    Normal = length([R || R <- Reasons, R =:= normal]),
    io:format("~b exited normally~n", [Normal]);
main(_) ->
    io:format("usage: ring SIZE ROUNDS~n"),
    erlang:halt(64).

%% Spawns processes N..1 so each knows its successor; returns P1 and monitors.
spawn_ring(Size, Main) -> spawn_ring(Size, Main, Main, []).

spawn_ring(0, _Main, Next, Monitors) ->
    {Next, Monitors};
spawn_ring(Index, Main, Next, Monitors) ->
    {Pid, Ref} = spawn_monitor(fun() -> relay(Index, Next, Main, 0) end),
    spawn_ring(Index - 1, Main, Pid, [{Pid, Ref} | Monitors]).

%% Forwards tokens and the stop signal; reports how many tokens it saw.
relay(Index, Next, Main, Seen) ->
    receive
        {token, Round, Hops} ->
            Next ! {token, Round, Hops + Index},
            relay(Index, Next, Main, Seen + 1);
        stop ->
            Next ! stop,
            Main ! {done, Index, Seen}
    end.

%% Sends one token per round and waits for it to come back around.
run_rounds(_First, Round, Rounds) when Round > Rounds ->
    ok;
run_rounds(First, Round, Rounds) ->
    First ! {token, Round, 0},
    receive
        {token, Round, Hops} -> io:format("round ~b: ~b hops~n", [Round, Hops])
    end,
    run_rounds(First, Round + 1, Rounds).

%% Receives the report of process Index regardless of arrival order.
collect(Index) ->
    receive
        {done, Index, Seen} -> Seen
    end.

await_down(Pid, Ref) ->
    receive
        {'DOWN', Ref, process, Pid, Reason} -> Reason
    end.

%% Returns at most the first Count elements.
take(0, _) -> [];
take(_, []) -> [];
take(Count, [Head | Tail]) -> [Head | take(Count - 1, Tail)].
