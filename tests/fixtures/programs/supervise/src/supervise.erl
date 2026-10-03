%% Supervision scenario: crashes, kills and normal exits of workers until the
%% supervisor gives up, plus links and monitors observed directly by main.
-module(supervise).
-export([main/1]).

%% Runs the scenario; only this process prints.
main(_Args) ->
    Sup = sup_tree:start([counter, echo], 3, self()),
    Monitor = monitor(process, Sup),
    receive
        {sup, started, Names} -> io:format("started ~w~n", [Names])
    end,
    ping(counter),
    ping(counter),
    ping(echo),
    crash(counter, {crashed, 1}),
    ping(counter),
    kill(echo),
    ping(echo),
    Sup ! {which_children, self()},
    receive
        {children, Children} -> io:format("children: ~w~n", [Children])
    end,
    crash(counter, normal),
    io:format("whereis counter: ~w~n", [whereis(counter)]),
    crash(echo, overload),
    crash(echo, overload),
    receive
        {sup, stopped, Left} -> io:format("stopped children: ~w~n", [Left])
    end,
    receive
        {'DOWN', Monitor, process, Sup, Reason} -> io:format("supervisor exit: ~w~n", [Reason])
    end,
    io:format("whereis echo: ~w~n", [whereis(echo)]),
    links().

%% Pings a registered worker and prints its counter.
ping(Name) ->
    Name ! {ping, self()},
    receive
        {pong, Name, Count} -> io:format("~w pong ~b~n", [Name, Count])
    end.

%% Asks a worker to exit with Reason and prints the supervisor's reaction.
crash(Name, Reason) ->
    whereis(Name) ! {crash, Reason},
    report(Name).

%% Kills a worker untrappably and prints the supervisor's reaction.
kill(Name) ->
    exit(whereis(Name), kill),
    report(Name).

report(Name) ->
    receive
        {sup, Event, Name, Reason, Restarts} ->
            io:format("~w ~w after ~w (restarts ~b)~n", [Name, Event, Reason, Restarts])
    end.

%% Traps exits from linked processes and checks demonitor flushing.
links() ->
    Old = process_flag(trap_exit, true),
    Quick = spawn_link(fun() -> exit({done, 7}) end),
    receive
        {'EXIT', Quick, Why} -> io:format("trapped (was ~w): ~w~n", [Old, Why])
    end,
    Idle = spawn_link(fun() ->
        receive
        after infinity -> ok
        end
    end),
    exit(Idle, kill),
    receive
        {'EXIT', Idle, Killed} -> io:format("linked kill: ~w~n", [Killed])
    end,
    Ref = monitor(process, Idle),
    receive
        {'DOWN', Ref, process, Idle, Gone} -> io:format("monitor dead pid: ~w~n", [Gone])
    end,
    Short = spawn(fun() -> ok end),
    Ref2 = monitor(process, Short),
    receive
    after 50 -> ok
    end,
    io:format("demonitor flush: ~w~n", [demonitor(Ref2, [flush, info])]),
    io:format("mailbox empty: ~w~n", [mailbox_empty()]).

%% Returns true when no message is left in the mailbox.
mailbox_empty() ->
    receive
        Any -> {unexpected, Any}
    after 0 -> true
    end.
