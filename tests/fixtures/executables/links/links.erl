-module(links).
-export([main/1, id/1]).

id(X) -> X.

% The exit reason of the linked process Pid, once its 'EXIT' message arrives.
exit_of(Pid) ->
    receive
        {'EXIT', Pid, Reason} -> Reason
    end.

% A process that waits for a stop message, replying to pings meanwhile.
waiter() ->
    receive
        {ping, From} ->
            From ! pong,
            waiter();
        stop ->
            ok
    end.

% Whether Pid still answers a ping.
answers(Pid) ->
    Pid ! {ping, self()},
    receive
        pong -> true
    after 1000 -> false
    end.

% Run F in a linked process and return its exit reason (the caller traps exits).
linked(F) -> exit_of(spawn_link(F)).

% A pid of a process that has ended.
dead() ->
    Pid = spawn_link(fun() -> ok end),
    normal = exit_of(Pid),
    Pid.

% A linked chain of N processes: the last one exits with Reason, which ends every process of the chain.
chain(0, Reason) ->
    exit(Reason);
chain(N, Reason) ->
    spawn_link(fun() -> chain(N - 1, Reason) end),
    waiter().

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(["main_killed"]) ->
    Main = self(),
    spawn(fun() -> exit(Main, kill) end),
    waiter();
main(["main_normal"]) ->
    io:format("before~n"),
    exit(self(), normal),
    io:format("not reached~n");
main(["main_linked"]) ->
    spawn_link(fun() -> exit(crashed) end),
    waiter();
main(_) ->
    % link/1 to an ended process raises noproc unless the caller traps exits.
    process_flag(trap_exit, true),
    Dead = dead(),
    Was = process_flag(trap_exit, false),
    io:format("trap_exit ~p ~p~n", [Was, process_flag(trap_exit, false)]),
    io:format("link dead ~p~n", [error_of(fun() -> link(Dead) end)]),
    io:format("trap_exit ~p~n", [process_flag(trap_exit, true)]),
    Linked = link(Dead),
    io:format("link dead trapping ~p ~p~n", [Linked, exit_of(Dead)]),
    % Exit reasons of linked processes reach a trapping process as messages.
    io:format("returned ~p~n", [linked(fun() -> ok end)]),
    io:format("exited ~p~n", [linked(fun() -> exit(foo) end)]),
    {boom, [_ | _]} = linked(fun() -> erlang:error(boom) end),
    {{nocatch, x}, [_ | _]} = linked(fun() -> throw(x) end),
    io:format("error and throw reasons carry stacks~n"),
    % exit(kill) from exit/1 is an ordinary reason: linked processes see kill, not killed.
    io:format("exit kill ~p~n", [linked(fun() -> exit(kill) end)]),
    io:format("middle ~p~n", [linked(fun() -> chain(1, kill) end)]),
    % A crash ends a linked chain process by process.
    io:format("chain ~p~n", [linked(fun() -> chain(100, gone) end)]),
    % exit/2 with kill ends even a trapping process, with reason killed.
    W1 = spawn_link(fun waiter/0),
    exit(W1, kill),
    io:format("killed ~p alive ~p~n", [exit_of(W1), is_process_alive(W1)]),
    Parent = self(),
    W2 = spawn_link(fun() ->
        process_flag(trap_exit, true),
        Parent ! ready,
        waiter()
    end),
    receive
        ready -> ok
    end,
    exit(W2, kill),
    io:format("trapping killed ~p~n", [exit_of(W2)]),
    % Other reasons reach a trapping process as messages, normal included.
    W3 = spawn_link(fun() ->
        process_flag(trap_exit, true),
        Parent ! ready,
        receive
            {'EXIT', Parent, R1} ->
                receive
                    {'EXIT', Parent, R2} -> exit({got, R1, R2})
                end
        end
    end),
    receive
        ready -> ok
    end,
    exit(W3, foo),
    exit(W3, normal),
    io:format("trapped ~p~n", [exit_of(W3)]),
    % Reason normal sent to another process that does not trap exits does nothing; other reasons end it.
    W4 = spawn_link(fun waiter/0),
    exit(W4, normal),
    io:format("normal ignored ~p~n", [answers(W4)]),
    exit(W4, bye),
    io:format("ended ~p~n", [exit_of(W4)]),
    % exit/2 to the caller itself: normal ends it (exit_signal/2 does not), kill passes every catch and after.
    io:format("self normal ~p~n", [
        linked(fun() ->
            exit(self(), normal),
            io:format("not reached~n")
        end)
    ]),
    io:format("exit_signal self normal ~p~n", [
        linked(fun() ->
            exit_signal(self(), normal),
            exit(survived)
        end)
    ]),
    io:format("self kill ~p~n", [
        linked(fun() ->
            try
                exit(self(), kill)
            catch
                _:_ -> io:format("not caught~n")
            after
                io:format("no after~n")
            end
        end)
    ]),
    io:format("self foo ~p~n", [
        linked(fun() ->
            try
                exit(self(), foo)
            catch
                _:_ -> io:format("not caught~n")
            end
        end)
    ]),
    io:format("self trapped ~p~n", [
        linked(fun() ->
            process_flag(trap_exit, true),
            Self = self(),
            exit(Self, foo),
            exit(Self, normal),
            receive
                {'EXIT', Self, foo} ->
                    receive
                        {'EXIT', Self, normal} -> exit(self(), kill)
                    end
            end
        end)
    ]),
    % unlink/1 removes the link on both sides.
    W5 = spawn_link(fun() ->
        receive
            crash ->
                Parent ! dying,
                exit(crash)
        end
    end),
    io:format("unlink ~p ~p~n", [unlink(W5), unlink(W5)]),
    W5 ! crash,
    receive
        dying -> ok
    end,
    Unlinked =
        receive
            {'EXIT', W5, _} -> linked
        after 100 -> unlinked
        end,
    io:format("~p~n", [Unlinked]),
    % A second link/1 to the same process adds nothing.
    W6 = spawn(fun waiter/0),
    io:format("links ~p ~p ~p~n", [link(W6), link(W6), link(self())]),
    W6 ! stop,
    io:format("stopped ~p~n", [exit_of(W6)]),
    io:format("~p~n", [
        [
            error_of(fun() -> link(?MODULE:id(foo)) end),
            error_of(fun() -> link(make_ref()) end),
            error_of(fun() -> unlink(?MODULE:id(foo)) end),
            error_of(fun() -> unlink(Dead) end),
            error_of(fun() -> exit(?MODULE:id(foo), bar) end),
            error_of(fun() -> exit(Dead, bar) end),
            error_of(fun() -> exit(make_ref(), bar) end),
            error_of(fun() -> exit_signal(?MODULE:id(1), bar) end),
            error_of(fun() -> process_flag(trap_exit, ?MODULE:id('maybe')) end),
            error_of(fun() -> spawn_link(?MODULE:id(foo)) end),
            error_of(fun() -> spawn_link(?MODULE:id(foo), bar, ?MODULE:id(baz)) end)
        ]
    ]),
    io:format("done~n").
