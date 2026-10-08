-module(sockets).
-export([main/1]).

% TCP and UDP sockets over loopback: passive and active modes, active once, packet framing, controlling process,
% closing from either side, errors, and IPv6.

show(Label, Value) -> io:format("~s ~p~n", [Label, Value]).

% A listening socket on a free loopback port, and the port number.
listen(Options) ->
    {ok, Listen} = gen_tcp:listen(0, [{ip, {127, 0, 0, 1}}, {reuseaddr, true} | Options]),
    {ok, Port} = inet:port(Listen),
    {Listen, Port}.

% Accept one connection on Listen in a new process that runs Serve on it, then closes it.
serve(Listen, Serve) ->
    Self = self(),
    spawn(fun() ->
        {ok, Socket} = gen_tcp:accept(Listen),
        Self ! accepted,
        Serve(Socket),
        gen_tcp:close(Socket)
    end),
    ok.

% Echo every passive read until the peer closes.
echo(Socket) ->
    case gen_tcp:recv(Socket, 0) of
        {ok, Data} ->
            gen_tcp:send(Socket, Data),
            echo(Socket);
        {error, closed} ->
            ok
    end.

% Messages that arrived for Socket until it closed or a pause.
messages(Socket) ->
    receive
        {tcp, Socket, Data} -> [{tcp, Data} | messages(Socket)];
        {tcp_closed, Socket} -> [tcp_closed];
        {tcp_error, Socket, Reason} -> [{tcp_error, Reason}]
    after 2000 -> [timeout]
    end.

% Read exactly Size bytes in passive mode.
read_all(Socket, Size) -> read_all(Socket, Size, <<>>).

read_all(_, 0, Acc) ->
    Acc;
read_all(Socket, Size, Acc) ->
    {ok, Data} = gen_tcp:recv(Socket, 0, 2000),
    read_all(Socket, Size - byte_size(Data), <<Acc/binary, Data/binary>>).

passive() ->
    {Listen, Port} = listen([binary, {active, false}]),
    serve(Listen, fun echo/1),
    {ok, Client} = gen_tcp:connect({127, 0, 0, 1}, Port, [binary, {active, false}]),
    receive
        accepted -> ok
    end,
    show("is_port", is_port(Client)),
    ok = gen_tcp:send(Client, <<"hello">>),
    ok = gen_tcp:send(Client, [" ", "world"]),
    show("echo", read_all(Client, 11)),
    show("recv timeout", gen_tcp:recv(Client, 0, 50)),
    {ok, {Address, _}} = inet:peername(Client),
    {ok, {Local, _}} = inet:sockname(Client),
    show("addresses", {Address, Local}),
    ok = gen_tcp:close(Client),
    show("send closed", gen_tcp:send(Client, <<"x">>)),
    show("recv closed", gen_tcp:recv(Client, 0)),
    gen_tcp:close(Listen).

active() ->
    {Listen, Port} = listen([binary, {packet, 2}, {active, false}]),
    serve(Listen, fun(Socket) ->
        [gen_tcp:send(Socket, Packet) || Packet <- [<<"one">>, <<"two">>, <<"three">>]],
        {ok, <<"bye">>} = gen_tcp:recv(Socket, 0)
    end),
    {ok, Client} = gen_tcp:connect("localhost", Port, [binary, {packet, 2}, {active, once}]),
    receive
        accepted -> ok
    end,
    First =
        receive
            {tcp, Client, D} -> D
        end,
    Quiet =
        receive
            {tcp, Client, _} -> more
        after 100 -> none
        end,
    ok = inet:setopts(Client, [{active, true}]),
    Next = [
        receive
            {tcp, Client, D2} -> D2
        end
     || _ <- [1, 2]
    ],
    ok = gen_tcp:send(Client, <<"bye">>),
    show("active once", {First, Quiet, Next, messages(Client)}),
    gen_tcp:close(Client),
    gen_tcp:close(Listen).

lists() ->
    {Listen, Port} = listen([list, {active, true}]),
    Self = self(),
    spawn(fun() ->
        {ok, Socket} = gen_tcp:accept(Listen),
        receive
            {tcp, Socket, Data} -> Self ! {server_got, Data}
        end,
        gen_tcp:send(Socket, "pong"),
        gen_tcp:close(Socket)
    end),
    {ok, Client} = gen_tcp:connect({127, 0, 0, 1}, Port, [list, {active, true}]),
    gen_tcp:send(Client, "ping"),
    receive
        {server_got, Got} -> show("server got", Got)
    end,
    show("client got", messages(Client)),
    gen_tcp:close(Listen).

owner() ->
    {Listen, Port} = listen([binary, {active, true}]),
    Self = self(),
    spawn(fun() ->
        {ok, Socket} = gen_tcp:accept(Listen),
        gen_tcp:send(Socket, <<"to the new owner">>),
        receive
            done -> gen_tcp:close(Socket)
        end
    end),
    {ok, Client} = gen_tcp:connect({127, 0, 0, 1}, Port, [binary, {active, true}]),
    Owner = spawn(fun() ->
        receive
            {tcp, Client, Data} -> Self ! {owner, Data}
        end,
        receive
            {tcp_closed, Client} -> Self ! {owner, tcp_closed}
        end
    end),
    show("controlling_process", gen_tcp:controlling_process(Client, Owner)),
    receive
        {owner, Data} -> show("owner got", Data)
    end,
    Other = spawn(fun() -> Self ! {other, gen_tcp:controlling_process(Client, self())} end),
    receive
        {other, Result} -> show("not owner", {Other =/= self(), Result})
    end,
    show("old owner", gen_tcp:controlling_process(Client, self())),
    gen_tcp:close(Client),
    gen_tcp:close(Listen).

errors() ->
    {Listen, Port} = listen([binary, {active, false}]),
    show("accept timeout", gen_tcp:accept(Listen, 50)),
    show("listen in use", gen_tcp:listen(Port, [{ip, {127, 0, 0, 1}}])),
    gen_tcp:close(Listen),
    show("accept closed", gen_tcp:accept(Listen, 50)),
    show(
        "bad option",
        try gen_tcp:listen(0, [{active, 'maybe'}]) of
            Value -> Value
        catch
            Class:Reason -> {Class, Reason}
        end
    ).

udp() ->
    {ok, A} = gen_udp:open(0, [binary, {ip, {127, 0, 0, 1}}, {active, false}]),
    {ok, B} = gen_udp:open(0, [binary, {ip, {127, 0, 0, 1}}, {active, true}]),
    {ok, PortA} = inet:port(A),
    {ok, PortB} = inet:port(B),
    ok = gen_udp:send(A, {127, 0, 0, 1}, PortB, <<"to b">>),
    Active =
        receive
            {udp, B, {127, 0, 0, 1}, PortA, Data} -> Data
        after 2000 -> timeout
        end,
    ok = gen_udp:send(B, {127, 0, 0, 1}, PortA, "to a"),
    {ok, {{127, 0, 0, 1}, From, Passive}} = gen_udp:recv(A, 0, 2000),
    show("udp", {Active, Passive, From =:= PortB, gen_udp:recv(A, 0, 50)}),
    gen_udp:close(A),
    gen_udp:close(B).

ipv6() ->
    {ok, Listen} = gen_tcp:listen(0, [
        inet6, {ip, {0, 0, 0, 0, 0, 0, 0, 1}}, binary, {active, false}
    ]),
    {ok, Port} = inet:port(Listen),
    serve(Listen, fun echo/1),
    {ok, Client} = gen_tcp:connect({0, 0, 0, 0, 0, 0, 0, 1}, Port, [inet6, binary, {active, false}]),
    ok = gen_tcp:send(Client, <<"six">>),
    {ok, {Peer, _}} = inet:peername(Client),
    show("ipv6", {read_all(Client, 3), Peer}),
    gen_tcp:close(Client),
    gen_tcp:close(Listen).

main(_) ->
    passive(),
    active(),
    lists(),
    owner(),
    errors(),
    udp(),
    ipv6(),
    io:format("done~n").
