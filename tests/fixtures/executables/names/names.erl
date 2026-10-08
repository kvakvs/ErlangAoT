-module(names).
-export([main/1, id/1]).

id(X) -> X.

% A process that answers {From, Message} with {got, Message} until it gets stop.
echo() ->
    receive
        {From, Message} ->
            From ! {got, Message},
            echo();
        stop ->
            ok
    end.

% The reply to the last request, or none.
reply() ->
    receive
        {got, Message} -> Message
    after 100 -> none
    end.

% The names among Names that are registered, sorted (OTP registers system processes too).
ours(Names) -> lists:sort([Name || Name <- registered(), lists:member(Name, Names)]).

% Stop Pid and wait until it has ended.
stop(Pid) ->
    Ref = monitor(process, Pid),
    Pid ! stop,
    receive
        {'DOWN', Ref, process, _, Reason} -> Reason
    end.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(_) ->
    Echo = spawn(fun echo/0),
    io:format("register ~p~n", [register(echo, Echo)]),
    io:format("whereis ~p ~p~n", [whereis(echo) =:= Echo, whereis(nobody)]),
    % A name reaches its process from every form of send.
    echo ! {self(), plain},
    io:format("~p~n", [reply()]),
    {echo, nonode@nohost} ! {self(), pair},
    io:format("~p~n", [reply()]),
    erlang:send(echo, {self(), function}),
    io:format("~p~n", [reply()]),
    % Messages to a name nothing on this node has are dropped.
    io:format("~p~n", [[{nobody, nonode@nohost} ! lost, {echo, 'other@host'} ! lost]]),
    Self = self(),
    register(main, Self),
    io:format("registered ~p~n", [ours([echo, main, other])]),
    Unregistered = unregister(main),
    io:format("unregister ~p ~p~n", [Unregistered, ours([echo, main, other])]),
    % A monitor made with a name reports {Name, Node}.
    Ref = monitor(process, echo),
    io:format("down ~p~n", [
        begin
            echo ! stop,
            receive
                {'DOWN', Ref, process, Item, Reason} -> {Item, Reason}
            end
        end
    ]),
    % The name is free again as soon as its process has ended.
    io:format("released ~p ~p~n", [whereis(echo), ours([echo])]),
    Echo2 = spawn(fun echo/0),
    Registered = register(echo, Echo2),
    io:format("reregister ~p ~p~n", [Registered, whereis(echo) =:= Echo2]),
    Ref2 = monitor(process, {echo, nonode@nohost}),
    exit(Echo2, kill),
    receive
        {'DOWN', Ref2, process, Item2, Reason2} -> io:format("killed ~p ~p~n", [Item2, Reason2])
    end,
    Ref3 = monitor(process, nobody),
    receive
        {'DOWN', Ref3, process, Item3, Reason3} ->
            io:format("unregistered ~p ~p~n", [Item3, Reason3])
    end,
    % A process may register itself; its name goes when it ends.
    Self2 = spawn(fun() ->
        register(self_named, self()),
        echo()
    end),
    Ref4 = monitor(process, Self2),
    Self2 ! {self(), ping},
    Pong = reply(),
    io:format("~p ~p~n", [Pong, whereis(self_named) =:= Self2]),
    exit(Self2, bye),
    receive
        {'DOWN', Ref4, process, Self2, bye} -> ok
    end,
    io:format("self named ~p~n", [whereis(self_named)]),
    Busy = spawn(fun echo/0),
    register(busy, Busy),
    Dead = spawn(fun() -> ok end),
    normal = stop(Dead),
    io:format("~p~n", [
        [
            error_of(fun() -> register(busy, self()) end),
            error_of(fun() -> register(another, Busy) end),
            error_of(fun() -> register(undefined, self()) end),
            error_of(fun() -> register(?MODULE:id("name"), self()) end),
            error_of(fun() -> register(name, ?MODULE:id(name)) end),
            error_of(fun() -> register(name, make_ref()) end),
            error_of(fun() -> register(name, Dead) end),
            error_of(fun() -> unregister(nobody) end),
            error_of(fun() -> unregister(?MODULE:id(1)) end),
            error_of(fun() -> whereis(?MODULE:id("busy")) end),
            error_of(fun() -> nobody ! message end),
            error_of(fun() -> {busy, ?MODULE:id(1)} ! message end),
            error_of(fun() -> monitor(process, {busy, 'other@host'}) end),
            error_of(fun() -> monitor(process, {busy, ?MODULE:id(1)}) end)
        ]
    ]),
    io:format("~p~n", [stop(Busy)]).
