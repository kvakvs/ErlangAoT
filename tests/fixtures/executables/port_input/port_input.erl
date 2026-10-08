-module(port_input).
-export([main/1]).

% Standard input read through an {fd, 0, 1} port: line and packet framing, end of input with and without eof,
% input racing receive timeouts, and a program that ends while its port is open.

% Print every message of P until {P, eof}.
messages(P) ->
    receive
        {P, eof} ->
            io:format("eof~n");
        {P, Message} ->
            io:format("~p~n", [Message]),
            messages(P)
    after 5000 -> io:format("timeout~n")
    end.

% The bytes of every data message of P until eof.
total(P, Bytes) ->
    receive
        {P, {data, Data}} -> total(P, Bytes + byte_size(Data));
        {P, eof} -> Bytes
    after 5000 -> {timeout, Bytes}
    end.

% Count lines while waiting each time with a timeout of 0 or 1 ms, which often expires before the next line.
race(P, Lines, Waits) ->
    receive
        {P, {data, {eol, _}}} -> race(P, Lines + 1, Waits + 1);
        {P, eof} -> Lines
    after Waits rem 2 -> race(P, Lines, Waits + 1)
    end.

main(["lines"]) ->
    P = open_port({fd, 0, 1}, [{line, 8}, eof, in]),
    messages(P),
    io:format("~p~n", [erlang:port_info(P, input)]);
main(["packets"]) ->
    P = open_port({fd, 0, 1}, [{packet, 2}, binary, eof, in]),
    messages(P);
main(["stream"]) ->
    P = open_port({fd, 0, 1}, [binary, eof, in]),
    io:format("~p bytes~n", [total(P, 0)]);
main(["close"]) ->
    % Without eof the end of input closes the port with reason normal.
    process_flag(trap_exit, true),
    P = open_port({fd, 0, 1}, [{line, 80}, in]),
    receive
        {P, Line} -> io:format("~p~n", [Line])
    end,
    receive
        {'EXIT', P, Reason} -> io:format("exit ~p ~p~n", [Reason, erlang:port_info(P)])
    end;
main(["race"]) ->
    P = open_port({fd, 0, 1}, [{line, 80}, eof, in]),
    io:format("~p lines~n", [race(P, 0, 0)]);
main(["teardown"]) ->
    open_port({fd, 0, 1}, [{line, 80}, eof, in]),
    io:format("main ends~n").
