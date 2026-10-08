-module(send).
-export([main/1, id/1, count/1]).

id(X) -> X.

count(0) -> ok;
count(N) -> count(N - 1).

wait(Pid) ->
    case is_process_alive(Pid) of
        true -> wait(Pid);
        false -> ok
    end.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(_) ->
    % The destination is evaluated before the message; a send's value is the message.
    Value =
        begin
            io:format("destination~n"),
            self()
        end !
            begin
                io:format("message~n"),
                {hello, [1, 2, 3]}
            end,
    io:format("~p~n", [Value]),
    Busy = spawn(send, count, [100000]),
    Dead = spawn(fun() -> ok end),
    wait(Dead),
    Big = lists:seq(1, 100000),
    io:format("~p~n", [
        [
            error_of(fun() -> Busy ! Big end) =:= {ok, Big},
            error_of(fun() -> erlang:send(Busy, {big, Big}) end) =:= {ok, {big, Big}},
            error_of(fun() -> Dead ! gone end),
            error_of(fun() -> self() ! self end),
            error_of(fun() -> erlang:'!'(self(), operator) end),
            error_of(fun() -> id({name, nonode@nohost}) ! dropped end),
            error_of(fun() -> id({name, 'other@host'}) ! dropped end),
            error_of(fun() -> id(unregistered) ! hi end),
            error_of(fun() -> erlang:send(id(unregistered), hi) end),
            error_of(fun() -> id(42) ! hi end),
            error_of(fun() -> id({1, 2}) ! hi end),
            error_of(fun() -> id({name, 1}) ! hi end),
            error_of(fun() -> id([]) ! hi end)
        ]
    ]),
    wait(Busy),
    io:format("done~n").
