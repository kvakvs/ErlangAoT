-module(selective_receive).
-export([main/1, id/1]).

id(X) -> X.

% Answer each ping with a pong until told to stop; the count of pings answered is the reply to stop.
ponger(Count) ->
    receive
        {ping, From, N} ->
            From ! {pong, N},
            ponger(Count + 1);
        {stop, From} ->
            From ! {stopped, Count}
    end.

ping(_Pong, 0) ->
    ok;
ping(Pong, N) ->
    Pong ! {ping, self(), N},
    receive
        {pong, N} -> ping(Pong, N - 1)
    end.

% Send Count numbered messages tagged with this sender to Parent.
producer(Parent, Tag, Count) ->
    [Parent ! {Tag, I} || I <- lists:seq(1, Count)],
    ok.

% Receive Count messages of one sender, which must arrive in order.
collect(_Tag, Count, Count) ->
    ok;
collect(Tag, Next, Count) ->
    receive
        {Tag, I} when I =:= Next + 1 -> collect(Tag, I, Count);
        {Tag, Other} -> {out_of_order, Tag, Next, Other}
    end.

% A counter server: a tail-recursive loop that keeps its state between messages.
counter(Total) ->
    receive
        {add, N} when is_integer(N) -> counter(Total + N);
        {get, From} ->
            From ! {total, Total},
            counter(Total);
        stop ->
            done
    end.

% Receive every message left in the mailbox, oldest first; there must be no wait at the end, so a final marker
% message is sent first.
drain(Acc) ->
    receive
        marker -> lists:reverse(Acc);
        Other -> drain([Other | Acc])
    end.

main(_) ->
    % Ping-pong: each side waits for the other.
    Pong = spawn(fun() -> ponger(0) end),
    ok = ping(Pong, 1000),
    Pong ! {stop, self()},
    receive
        {stopped, Answered} -> io:format("pong answered ~p~n", [Answered])
    end,
    % Fan-in: three senders interleave, each sender's messages arrive in order.
    Self = self(),
    [spawn(fun() -> producer(Self, Tag, 500) end) || Tag <- [a, b, c]],
    io:format("fan-in ~p~n", [[collect(Tag, 0, 500) || Tag <- [c, a, b]]]),
    % Self-send and out-of-order selection: unmatched messages stay in order.
    self() ! one,
    self() ! two,
    self() ! three,
    Third =
        receive
            three -> 3
        end,
    First =
        receive
            one -> 1
        end,
    Second =
        receive
            X when X =:= two -> 2
        end,
    io:format("selected ~p~n", [[Third, First, Second]]),
    % A bound variable in a pattern selects only its value; names bound in every clause are exported.
    Ref = make_ref(),
    self() ! {other, wrong},
    self() ! {Ref, right},
    receive
        {Ref, Value} -> ok
    end,
    receive
        {other, Left} -> ok
    end,
    io:format("bound ~p ~p~n", [Value, Left]),
    % Many unmatched messages stay behind the wanted one and keep their order.
    [self() ! {junk, I} || I <- lists:seq(1, 10000)],
    self() ! wanted,
    receive
        wanted -> ok
    end,
    self() ! marker,
    Junk = drain([]),
    io:format("junk ~p ~p ~p~n", [length(Junk), hd(Junk), lists:nth(10000, Junk)]),
    % A process that has no matching message waits until one arrives.
    Counter = spawn(fun() -> counter(0) end),
    [Counter ! {add, N} || N <- lists:seq(1, 100)],
    Counter ! {add, not_a_number},
    Counter ! {get, self()},
    receive
        {total, Total} -> io:format("total ~p~n", [Total])
    end,
    Counter ! stop,
    % A process still waiting for a message when the main process returns does not keep the program alive.
    spawn(fun() ->
        receive
            never -> ok
        end
    end),
    io:format("main done ~p~n", [id(ok)]).
