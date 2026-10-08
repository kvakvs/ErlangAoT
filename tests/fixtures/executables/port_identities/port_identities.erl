-module(port_identities).
-export([main/1, id/1]).

% Port identities, ownership, links, monitors, names and exit signals over output-only fd ports on standard output,
% which are never written: only one is open at a time.

id(X) -> X.

fd() -> open_port({fd, 0, 1}, [out]).

% Messages that arrived, oldest first.
flush() ->
    receive
        M -> [M | flush()]
    after 50 -> []
    end.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

% Text with each run of digits replaced by N, so output does not depend on numbering.
digits(Text) -> digits(Text, false).

digits([C | Rest], InNumber) when C >= $0, C =< $9 ->
    case InNumber of
        true -> digits(Rest, true);
        false -> [$N | digits(Rest, true)]
    end;
digits([C | Rest], _) ->
    [C | digits(Rest, false)];
digits([], _) ->
    [].

% The category of a term, to show term order without identity numbers.
kind(X) when is_integer(X) -> integer;
kind(X) when is_atom(X) -> atom;
kind(X) when is_reference(X) -> reference;
kind(X) when is_function(X) -> function;
kind(X) when is_port(X) -> port;
kind(X) when is_pid(X) -> pid.

% Wait until Pid has ended.
gone(Pid) ->
    Ref = monitor(process, Pid),
    receive
        {'DOWN', Ref, process, Pid, _} -> ok
    end.

identity() ->
    P = fd(),
    io:format("~p ~s~n", [[is_port(P), is_pid(P), is_reference(P)], digits(port_to_list(P))]),
    io:format("~p~n", [[list_to_port(port_to_list(P)) =:= P, lists:member(P, erlang:ports())]]),
    io:format("~p~n", [[kind(X) || X <- lists:sort([self(), P, make_ref(), fun id/1, 1, a])]]),
    io:format("~p~n", [[K || {K, _} <- erlang:port_info(P)]]),
    Self = self(),
    io:format("~p~n", [
        [
            erlang:port_info(P, name),
            erlang:port_info(P, links) =:= {links, [Self]},
            erlang:port_info(P, connected) =:= {connected, Self},
            erlang:port_info(P, input),
            erlang:port_info(P, output),
            erlang:port_info(P, os_pid),
            erlang:port_info(P, monitors),
            erlang:port_info(P, monitored_by),
            erlang:port_info(P, registered_name)
        ]
    ]),
    io:format("close ~p ~p~n", [port_close(P), flush()]),
    io:format("closed ~p~n", [
        [
            is_port(P),
            erlang:port_info(P),
            erlang:port_info(P, name),
            lists:member(P, erlang:ports()),
            error_of(fun() -> port_close(P) end),
            error_of(fun() -> port_command(P, "x") end),
            error_of(fun() -> port_connect(P, self()) end)
        ]
    ]),
    P.

signals() ->
    P1 = fd(),
    R1 = monitor(port, P1),
    io:format("monitored_by ~p~n", [erlang:port_info(P1, monitored_by) =:= {monitored_by, [self()]}]),
    port_close(P1),
    [{'EXIT', P1, normal}, {'DOWN', R1, port, P1, normal}] = flush(),
    P2 = fd(),
    exit(P2, kill),
    io:format("kill ~p ~p~n", [[R || {'EXIT', _, R} <- flush()], erlang:port_info(P2)]),
    P3 = fd(),
    exit(P3, normal),
    io:format("normal ~p ~p~n", [[R || {'EXIT', _, R} <- flush()], erlang:port_info(P3)]),
    P4 = fd(),
    exit(P4, {some, reason}),
    io:format("term ~p ~p~n", [[R || {'EXIT', _, R} <- flush()], erlang:port_info(P4)]),
    P5 = fd(),
    P5 ! foo,
    io:format("badsig ~p ~p~n", [
        [R || {'EXIT', _, R} <- flush()], erlang:port_info(P5) =/= undefined
    ]),
    port_close(P5),
    flush().

names() ->
    P = fd(),
    io:format("register ~p ~p ~p ~p~n", [
        register(myport, P),
        whereis(myport) =:= P,
        erlang:port_info(P, registered_name),
        lists:member(myport, registered())
    ]),
    Ref = monitor(port, myport),
    myport ! {self(), close},
    io:format("named close ~p ~p~n", [
        [
            case M of
                {'EXIT', P, R} -> {exit, R};
                {P, closed} -> closed;
                {'DOWN', Ref, port, {myport, nonode@nohost}, R} -> {down, R}
            end
         || M <- flush()
        ],
        whereis(myport)
    ]).

owners() ->
    Self = self(),
    Owner = spawn(fun() ->
        receive
            {port, Q} -> Self ! {got, erlang:port_info(Q, connected)}
        end,
        receive
            stop -> ok
        end
    end),
    P = fd(),
    P ! {self(), {connect, Owner}},
    io:format("connect ~p~n", [[M || {Q, M} <- flush(), Q =:= P]]),
    Owner ! {port, P},
    receive
        {got, Connected} -> io:format("new owner ~p~n", [Connected =:= {connected, Owner}])
    end,
    io:format("port_connect ~p ~p~n", [
        port_connect(P, self()), erlang:port_info(P, connected) =:= {connected, Self}
    ]),
    Owner ! stop,
    gone(Owner),
    port_close(P),
    flush(),
    % The port of a linked opener closes when it ends, even normally.
    Opener = spawn(fun() ->
        Self ! {opened, fd()},
        receive
            stop -> ok
        end
    end),
    P2 =
        receive
            {opened, Q2} -> Q2
        end,
    R2 = monitor(port, P2),
    Opener ! stop,
    receive
        {'DOWN', R2, port, P2, Reason2} -> io:format("opener ended ~p~n", [Reason2])
    end,
    % An opener that unlinked leaves its port open when it ends; any process may close it.
    Unlinked = spawn(fun() ->
        Q3 = fd(),
        unlink(Q3),
        Self ! {opened, Q3},
        receive
            stop -> ok
        end
    end),
    P3 =
        receive
            {opened, Q3} -> Q3
        end,
    Unlinked ! stop,
    gone(Unlinked),
    io:format("unlinked opener ended ~p ~p~n", [erlang:port_info(P3) =/= undefined, port_close(P3)]),
    % A linked process ending normally leaves the port open; another reason closes it.
    P4 = fd(),
    Linker = fun(Reason) ->
        spawn(fun() ->
            link(P4),
            Self ! linked,
            receive
                go -> exit(Reason)
            end
        end)
    end,
    Normal = Linker(normal),
    receive
        linked -> Normal ! go
    end,
    gone(Normal),
    io:format("linked normal ~p~n", [erlang:port_info(P4) =/= undefined]),
    Boom = Linker(boom),
    receive
        linked -> Boom ! go
    end,
    gone(Boom),
    io:format("linked boom ~p ~p~n", [erlang:port_info(P4), [R || {'EXIT', _, R} <- flush()]]).

closed(P) ->
    R = monitor(port, P),
    R2 = monitor(port, nobody),
    io:format("monitor closed ~p~n", [
        [
            case M of
                {'DOWN', R, port, P, Why} -> {port, Why};
                {'DOWN', R2, port, Item, Why} -> {Item, Why}
            end
         || M <- flush()
        ]
    ]),
    io:format("link closed ~p ~p~n", [link(P), [R3 || {'EXIT', _, R3} <- flush()]]),
    process_flag(trap_exit, false),
    io:format("link closed ~p~n", [error_of(fun() -> link(P) end)]),
    process_flag(trap_exit, true).

errors() ->
    P = fd(),
    io:format("~p~n", [
        [
            error_of(fun() -> open_port(?MODULE:id(foo), []) end),
            error_of(fun() -> open_port({fd, ?MODULE:id(a), 1}, []) end),
            error_of(fun() -> open_port({fd, 0, 1}, [?MODULE:id(bogus)]) end),
            error_of(fun() -> port_command(P, [], [nosuspend]) end),
            error_of(fun() -> port_command(P, [], [force]) end),
            error_of(fun() -> port_command(P, [], [bogus]) end),
            error_of(fun() -> port_control(P, 0, []) end),
            error_of(fun() -> erlang:port_call(P, 0, []) end),
            error_of(fun() -> erlang:port_info(self()) end),
            error_of(fun() -> erlang:port_info(P, bogus) end),
            error_of(fun() -> list_to_port("#Port<0.x>") end),
            error_of(fun() -> port_to_list(self()) end),
            error_of(fun() -> monitor(process, P) end),
            error_of(fun() -> monitor(port, self()) end),
            error_of(fun() -> port_connect(P, ?MODULE:id(foo)) end),
            error_of(fun() -> is_process_alive(?MODULE:id(P)) end),
            error_of(fun() -> link(?MODULE:id(foo)) end)
        ]
    ]),
    port_close(P),
    flush().

main(_) ->
    process_flag(trap_exit, true),
    P = identity(),
    signals(),
    names(),
    owners(),
    closed(P),
    errors(),
    io:format("done~n").
