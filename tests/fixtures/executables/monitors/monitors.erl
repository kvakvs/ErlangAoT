-module(monitors).
-export([main/1, id/1]).

id(X) -> X.

% A process that waits for one message, then exits with it as its reason.
waiter() ->
    receive
        Reason -> exit(Reason)
    end.

% The reason of the 'DOWN' message of monitor Ref on Pid.
down(Ref, Pid) ->
    receive
        {'DOWN', Ref, process, Pid, Reason} -> Reason
    end.

sleep(Milliseconds) ->
    receive
    after Milliseconds -> ok
    end.

% Every message already in the mailbox.
drain() ->
    receive
        Message -> [Message | drain()]
    after 0 -> []
    end.

% A pid of a process that has ended.
dead() ->
    {Pid, Ref} = spawn_monitor(fun() -> ok end),
    normal = down(Ref, Pid),
    Pid.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(_) ->
    % 'DOWN' carries the exit reason of the monitored process.
    {P1, R1} = spawn_monitor(fun() -> ok end),
    io:format("returned ~p~n", [down(R1, P1)]),
    {P2, R2} = spawn_monitor(fun() -> exit(bye) end),
    io:format("exited ~p~n", [down(R2, P2)]),
    {P3, R3} = spawn_monitor(fun() -> erlang:error(boom) end),
    {boom, [_ | _]} = down(R3, P3),
    {P4, R4} = spawn_monitor(?MODULE, id, [1, 2]),
    {undef, _} = down(R4, P4),
    io:format("error reasons carry stacks~n"),
    % monitor/2 of a live process; exit(Pid, kill) shows as killed.
    P5 = spawn(fun waiter/0),
    R5 = monitor(process, P5),
    exit(P5, kill),
    io:format("killed ~p~n", [down(R5, P5)]),
    % A process that has ended answers at once with noproc.
    Dead = dead(),
    R6 = monitor(process, Dead),
    io:format("noproc ~p~n", [down(R6, Dead)]),
    % Each monitor gets its own 'DOWN' message, in the order they were made.
    P7 = spawn(fun waiter/0),
    R7a = monitor(process, P7),
    R7b = monitor(process, P7),
    P7 ! twice,
    [{'DOWN', A, process, P7, twice}, {'DOWN', B, process, P7, twice}] = [
        receive
            M1 = {'DOWN', _, _, _, _} -> M1
        end,
        receive
            M2 = {'DOWN', _, _, _, _} -> M2
        end
    ],
    io:format("two monitors ~p~n", [lists:sort([A, B]) =:= lists:sort([R7a, R7b])]),
    % demonitor/1 stops the monitor: no 'DOWN' arrives after it.
    P8 = spawn(fun waiter/0),
    R8 = monitor(process, P8),
    io:format("demonitor ~p~n", [demonitor(R8)]),
    R8b = monitor(process, P8),
    P8 ! stop,
    stop = down(R8b, P8),
    io:format("after demonitor ~p~n", [drain()]),
    % info tells whether the monitor was still active; flush removes a 'DOWN' already delivered.
    P9 = spawn(fun waiter/0),
    R9 = monitor(process, P9),
    Found = demonitor(R9, [info]),
    io:format("info ~p ~p~n", [Found, demonitor(R9, [info])]),
    P9 ! stop,
    {_, R10} = spawn_monitor(fun() -> ok end),
    sleep(50),
    io:format("flush ~p ~p~n", [demonitor(R10, [flush, info]), drain()]),
    % Without flush the 'DOWN' stays; flush of an active monitor leaves other messages alone.
    {P11, R11} = spawn_monitor(fun() -> ok end),
    sleep(50),
    io:format("no flush ~p~n", [demonitor(R11, [info])]),
    [{'DOWN', R11, process, P11, normal}] = drain(),
    P12 = spawn(fun waiter/0),
    R12 = monitor(process, P12),
    self() ! {tag, R12, a, b, c},
    io:format("active flush ~p~n", [demonitor(R12, [flush])]),
    [{tag, R12, a, b, c}] = drain(),
    P12 ! stop,
    % Monitoring itself does nothing.
    R13 = monitor(process, self()),
    io:format("self ~p ~p~n", [demonitor(R13, [info]), drain()]),
    % A monitoring process that ends drops its monitors.
    P14 = spawn(fun waiter/0),
    Parent = self(),
    {Watcher, RW} = spawn_monitor(fun() ->
        monitor(process, P14),
        Parent ! watching
    end),
    receive
        watching -> ok
    end,
    normal = down(RW, Watcher),
    R14 = monitor(process, P14),
    P14 ! done,
    io:format("watcher ended ~p ~p~n", [down(R14, P14), drain()]),
    io:format("~p~n", [
        [
            error_of(fun() -> monitor(?MODULE:id(port), self()) end),
            error_of(fun() -> monitor(?MODULE:id(foo), self()) end),
            error_of(fun() -> monitor(process, ?MODULE:id(1)) end),
            error_of(fun() -> monitor(process, make_ref()) end),
            error_of(fun() -> demonitor(?MODULE:id(foo)) end),
            error_of(fun() -> demonitor(make_ref()) end),
            error_of(fun() -> demonitor(make_ref(), [info]) end),
            error_of(fun() -> demonitor(make_ref(), [flush, info]) end),
            error_of(fun() -> demonitor(make_ref(), []) end),
            error_of(fun() -> demonitor(make_ref(), [?MODULE:id(foo)]) end),
            error_of(fun() -> demonitor(make_ref(), ?MODULE:id(flush)) end),
            error_of(fun() -> demonitor(make_ref(), [flush | ?MODULE:id(info)]) end),
            error_of(fun() -> spawn_monitor(?MODULE:id(foo)) end),
            error_of(fun() -> spawn_monitor(?MODULE:id(foo), bar, ?MODULE:id(baz)) end)
        ]
    ]),
    io:format("done~n").
