%% Registered key-value server process with a synchronous call protocol.
-module(kv_server).
-export([start/0, store/2, cast_store/2, fetch/1, remove/1, keys/0, incr/2]).
-export([stats/0, slow/2, stop/0]).

%% Spawns the server loop and registers it as kv_server.
start() ->
    Pid = spawn(fun() -> loop(#{}, 0) end),
    true = register(kv_server, Pid),
    Pid.

store(Key, Value) -> call({store, Key, Value}).
fetch(Key) -> call({fetch, Key}).
remove(Key) -> call({remove, Key}).
keys() -> call(keys).
incr(Key, By) -> call({incr, Key, By}).
stats() -> call(stats).
stop() -> call(stop).

%% Asks the server to wait Delay ms before replying; the client waits Timeout ms.
slow(Delay, Timeout) -> call({slow, Delay}, Timeout).

%% Stores without waiting for a reply.
cast_store(Key, Value) ->
    kv_server ! {cast, {store, Key, Value}},
    ok.

call(Request) -> call(Request, 1000).

%% Sends a tagged request and selectively waits for the matching reply.
call(Request, Timeout) ->
    Ref = make_ref(),
    kv_server ! {call, self(), Ref, Request},
    receive
        {reply, Ref, Reply} -> Reply
    after Timeout -> {timeout, Ref}
    end.

%% Serves requests until stop; Ops counts handled requests.
loop(Data, Ops) ->
    receive
        {call, From, Ref, stop} ->
            From ! {reply, Ref, {stopped, map_size(Data), Ops}};
        {call, From, Ref, stats} ->
            From ! {reply, Ref, {map_size(Data), Ops}},
            loop(Data, Ops + 1);
        {call, From, Ref, {slow, Delay}} ->
            receive
            after Delay -> From ! {reply, Ref, late}
            end,
            loop(Data, Ops + 1);
        {call, From, Ref, Request} ->
            {Reply, Data1} = handle(Request, Data),
            From ! {reply, Ref, Reply},
            loop(Data1, Ops + 1);
        {cast, {store, Key, Value}} ->
            loop(Data#{Key => Value}, Ops + 1)
    end.

%% Computes the reply and new state for one request.
handle({store, Key, Value}, Data) ->
    {ok, Data#{Key => Value}};
handle({fetch, Key}, Data) ->
    {maps:find(Key, Data), Data};
handle({remove, Key}, Data) ->
    {ok, #{K => V || K := V <- Data, K =/= Key}};
handle(keys, Data) ->
    {lists:sort(maps:keys(Data)), Data};
handle({incr, Key, By}, Data) ->
    case Data of
        #{Key := Old} when is_integer(Old) -> {{ok, Old + By}, Data#{Key := Old + By}};
        #{Key := _} -> {{error, not_integer}, Data};
        #{} -> {{ok, By}, Data#{Key => By}}
    end;
handle(Other, Data) ->
    {{error, {unknown, Other}}, Data}.
