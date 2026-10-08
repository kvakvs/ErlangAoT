-module(many_ports).
-export([main/1]).

% Thousands of ports open at once: a thousand loopback TCP connections (a client and an accepted socket each, all
% open together) and concurrent spawned programs; every connection echoes and every program answers.

-define(CONNECTIONS, 1000).
-define(PROGRAMS, 32).

main(_) ->
    {ok, Listen} = gen_tcp:listen(0, [
        binary, {active, false}, {ip, {127, 0, 0, 1}}, {reuseaddr, true}, {backlog, 1024}
    ]),
    {ok, Port} = inet:port(Listen),
    Self = self(),
    spawn(fun() -> acceptor(Listen, Self) end),
    Clients = [spawn(fun() -> client(Port, N, Self) end) || N <- lists:seq(1, ?CONNECTIONS)],
    wait(connected, ?CONNECTIONS),
    wait(accepted, ?CONNECTIONS),
    io:format("open ~p~n", [length(erlang:ports()) > 2 * ?CONNECTIONS]),
    io:format("programs ~p~n", [programs()]),
    [Client ! go || Client <- Clients],
    io:format("echoed ~p~n", [count(echoed, ?CONNECTIONS, 0)]).

% Accept connections for ever, each echoed by the accepting process while a new one accepts.
acceptor(Listen, Main) ->
    {ok, Socket} = gen_tcp:accept(Listen),
    spawn(fun() -> acceptor(Listen, Main) end),
    Main ! accepted,
    echo(Socket).

% Echo one message, then close first, so the closing connection waits on the server's side and the host keeps the
% clients' ports free.
echo(Socket) ->
    {ok, Data} = gen_tcp:recv(Socket, 4),
    ok = gen_tcp:send(Socket, Data),
    ok = gen_tcp:close(Socket).

% Connect, wait until every connection is open, then send N, check the echo and see the server close.
client(Port, N, Main) ->
    {ok, Socket} = connect(Port, 100),
    Main ! connected,
    receive
        go -> ok
    end,
    ok = gen_tcp:send(Socket, <<N:32>>),
    {ok, <<N:32>>} = gen_tcp:recv(Socket, 4, 10000),
    {error, closed} = gen_tcp:recv(Socket, 0, 10000),
    ok = gen_tcp:close(Socket),
    Main ! echoed.

% Connect, trying again while a loaded host refuses a connection or runs short of local ports for a moment.
connect(Port, Tries) ->
    case gen_tcp:connect({127, 0, 0, 1}, Port, [binary, {active, false}]) of
        {ok, Socket} ->
            {ok, Socket};
        {error, _} when Tries > 1 ->
            receive
            after 10 -> connect(Port, Tries - 1)
            end
    end.

% Wait for Count messages Tag.
wait(_, 0) ->
    ok;
wait(Tag, Count) ->
    receive
        Tag -> wait(Tag, Count - 1)
    end.

% The number of messages Tag among the next Count.
count(_, 0, Seen) ->
    Seen;
count(Tag, Count, Seen) ->
    receive
        Tag -> count(Tag, Count - 1, Seen + 1)
    end.

% Open every program at once, then ask each for a line: the number of right answers.
programs() ->
    Python = os:getenv("CLAUSE_TEST_PYTHON"),
    Ports = [
        {N, open_port({spawn_executable, Python}, [{args, ["helper.py"]}, {line, 80}])}
     || N <- lists:seq(1, ?PROGRAMS)
    ],
    [port_command(P, integer_to_list(N) ++ "\n") || {N, P} <- Ports],
    Answers = [answer(N, P) || {N, P} <- Ports],
    [port_close(P) || {_, P} <- Ports],
    length([ok || ok <- Answers]).

% Whether port P answered N.
answer(N, P) ->
    Expected = integer_to_list(N),
    receive
        {P, {data, {eol, Expected}}} -> ok
    after 30000 -> timeout
    end.
