-module(port_fairness).
-export([main/1]).

% Ports flooding input share the workers with processes: while two spawned programs write lines as fast as they
% can, more CPU-bound processes than workers finish their loops long before the floods have been delivered, and two
% processes exchange a thousand messages. A scheduler that ran ports first would deliver both floods before.

-define(PINGS, 1000).
-define(LOOP, 1000000).
% More CPU-bound processes than workers, so port tasks always compete with runnable processes.
-define(CPUS, 8).

main(_) ->
    Self = self(),
    Owners = [spawn(fun() -> flood(Self) end) || _ <- [1, 2]],
    Ports = [
        receive
            {flooding, Owner, Port} -> Port
        end
     || Owner <- Owners
    ],
    [spawn(fun() -> Self ! {cpu, loop(?LOOP, 0)} end) || _ <- lists:seq(1, ?CPUS)],
    Pong = spawn(fun pong/0),
    spawn(fun() -> Self ! {pings, ping(Pong, ?PINGS, 0)} end),
    io:format("cpu ~p~n", [sums(?CPUS, 0)]),
    % The floods stop as soon as the CPU-bound processes are done; the pings go on after them.
    [port_close(Port) || Port <- Ports],
    [Owner ! stop || Owner <- Owners],
    [
        receive
            {flooded, Owner, Lines, Ended} ->
                io:format("flooded ~p, still flooding ~p~n", [Lines > 0, not Ended])
        end
     || Owner <- Owners
    ],
    receive
        {pings, Count} -> io:format("pings ~p~n", [Count])
    end.

% Open a flooding program, tell Main once its input flows, then count its lines until told to stop.
flood(Main) ->
    Port = open_port({spawn_executable, os:getenv("CLAUSE_TEST_PYTHON")}, [
        {args, ["helper.py"]}, {line, 100}, binary
    ]),
    receive
        {Port, {data, _}} -> Main ! {flooding, self(), Port}
    end,
    count(Main, Port, 1, false).

% Count the lines until told to stop, noting whether the flood's last line arrived.
count(Main, Port, Lines, Ended) ->
    receive
        {Port, {data, {eol, <<"end">>}}} -> count(Main, Port, Lines, true);
        {Port, {data, _}} -> count(Main, Port, Lines + 1, Ended);
        stop -> Main ! {flooded, self(), Lines, Ended}
    end.

% The sum of Count results of the CPU-bound processes.
sums(0, Total) ->
    Total;
sums(Count, Total) ->
    receive
        {cpu, Sum} -> sums(Count - 1, Total + Sum)
    end.

loop(0, Sum) -> Sum;
loop(N, Sum) -> loop(N - 1, Sum + N rem 7).

ping(_, 0, Count) ->
    Count;
ping(Pong, N, Count) ->
    Pong ! {ping, self()},
    receive
        pong -> ping(Pong, N - 1, Count + 1)
    end.

pong() ->
    receive
        {ping, From} ->
            From ! pong,
            pong()
    end.
