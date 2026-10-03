%% Drives the key-value server through stores, counters, timeouts, selective
%% receive and shutdown.
-module(kvstore).
-export([main/1]).

%% Runs the client scenario and prints each reply.
main(_Args) ->
    Pid = kv_server:start(),
    Monitor = monitor(process, Pid),
    io:format("registered: ~w~n", [whereis(kv_server) =:= Pid]),
    ok = kv_server:store(apple, 3),
    ok = kv_server:store(<<"pear">>, {box, 12}),
    ok = kv_server:store(42, "answer"),
    ok = kv_server:cast_store(plum, 7.5),
    show(fetch_apple, kv_server:fetch(apple)),
    show(fetch_plum, kv_server:fetch(plum)),
    show(fetch_fig, kv_server:fetch(fig)),
    show(incr_apple, kv_server:incr(apple, 10)),
    show(incr_pear, kv_server:incr(<<"pear">>, 1)),
    show(incr_new, kv_server:incr(counter, -4)),
    show(keys, kv_server:keys()),
    show(remove_plum, kv_server:remove(plum)),
    show(keys, kv_server:keys()),
    self() ! {note, first},
    self() ! {note, second},
    show(stats, kv_server:stats()),
    {timeout, Ref} = kv_server:slow(300, 20),
    io:format("slow call timed out~n"),
    receive
        {reply, Ref, Late} -> io:format("late reply: ~w~n", [Late])
    end,
    io:format("unmatched mail: ~w~n", [drain()]),
    show(stop, kv_server:stop()),
    receive
        {'DOWN', Monitor, process, Pid, Reason} -> io:format("server exit: ~w~n", [Reason])
    end,
    io:format("whereis after stop: ~w~n", [whereis(kv_server)]),
    show(call_after_stop, try_fetch(apple)).

%% Prints one labelled reply.
show(Label, Reply) -> io:format("~w: ~w~n", [Label, Reply]).

%% Collects every message already in the mailbox, oldest first.
drain() ->
    receive
        Message -> [Message | drain()]
    after 0 -> []
    end.

%% Sending to an unregistered name raises badarg.
try_fetch(Key) ->
    try kv_server:fetch(Key) of
        Reply -> {unexpected, Reply}
    catch
        error:badarg -> badarg
    end.
