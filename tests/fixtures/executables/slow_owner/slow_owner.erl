-module(slow_owner).
-export([main/1]).

% A program floods a port with lines while the port's owner computes before it receives anything: the port stops
% taking input the owner has not taken, and every line still arrives, in order.

-define(LINES, 100000).

main(_) ->
    Port = open_port({spawn_executable, os:getenv("CLAUSE_TEST_PYTHON")}, [
        {args, ["helper.py", integer_to_list(?LINES)]}, {line, 100}, binary
    ]),
    io:format("computed ~p~n", [loop(3000000, 0)]),
    io:format("in order ~p~n", [lines(Port, 1)]),
    port_close(Port).

loop(0, Sum) -> Sum;
loop(N, Sum) -> loop(N - 1, Sum + N rem 7).

% Receive lines 1 to ?LINES and the end line; true when each came in order.
lines(Port, N) when N > ?LINES ->
    receive
        {Port, {data, {eol, <<"end">>}}} -> true
    end;
lines(Port, N) ->
    Expected = list_to_binary(integer_to_list(N)),
    receive
        {Port, {data, {eol, Expected}}} -> lines(Port, N + 1);
        {Port, {data, Other}} -> {N, Other}
    end.
