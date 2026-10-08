-module(receive_after).
-export([main/1, id/1]).

id(X) -> X.

% timer:sleep/1 built on receive ... after.
sleep(Milliseconds) ->
    receive
    after Milliseconds -> ok
    end.

% Take every message already in the mailbox without waiting.
drain(Acc) ->
    receive
        Message -> drain([Message | Acc])
    after 0 -> lists:reverse(Acc)
    end.

% Send Message to Pid after Delay milliseconds.
later(Pid, Delay, Message) ->
    spawn(fun() ->
        sleep(Delay),
        Pid ! Message
    end).

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(_) ->
    Self = self(),
    % after 0 polls: with no matching message the after body runs at once, and messages stay in order.
    self() ! a,
    self() ! b,
    Poll =
        receive
            c -> c
        after 0 -> none
        end,
    io:format("poll ~p, drained ~p~n", [Poll, drain([])]),
    % A finite timeout expires when no message matches; the after body sees the bindings before the receive.
    Before = before,
    Expired =
        receive
            never -> never
        after 20 -> {expired, Before}
        end,
    io:format("~p~n", [Expired]),
    % A message arriving before the timeout expires is taken, never lost.
    later(Self, 10, early),
    Early =
        receive
            early -> early
        after 5000 -> too_late
        end,
    io:format("~p~n", [Early]),
    % infinity waits until the message arrives.
    later(Self, 10, eventually),
    Eventually =
        receive
            eventually -> eventually
        after infinity -> never
        end,
    io:format("~p~n", [Eventually]),
    % Sleeps end in order of their length.
    [
        spawn(fun() ->
            sleep(Delay),
            Self ! {woke, Delay}
        end)
     || Delay <- [60, 20, 40]
    ],
    io:format("woke ~p~n", [
        [
            receive
                {woke, D} -> D
            end
         || _ <- [1, 2, 3]
        ]
    ]),
    % Messages that do not match do not restart the timeout: it expires while they keep arriving.
    Spammer = spawn(fun() ->
        [
            begin
                sleep(30),
                Self ! {junk, I}
            end
         || I <- lists:seq(1, 10)
        ],
        Self ! done
    end),
    Spammed =
        receive
            wanted -> wanted
        after 100 -> {timeout, is_process_alive(Spammer)}
        end,
    io:format("~p~n", [Spammed]),
    receive
        done -> ok
    end,
    io:format("junk ~p~n", [length(drain([]))]),
    % Names bound in every clause and the after body are exported.
    self() ! {value, 1},
    receive
        {value, X} -> ok
    after 0 -> X = none
    end,
    receive
        {value, Y} -> ok
    after 0 -> Y = none
    end,
    io:format("exported ~p ~p~n", [X, Y]),
    % Invalid timeouts raise timeout_value, but only when the receive would wait.
    self() ! m,
    io:format("~p~n", [
        [
            error_of(fun() ->
                receive
                    m -> got
                after id(foo) -> ok
                end
            end),
            error_of(fun() ->
                receive
                after id(foo) -> ok
                end
            end),
            error_of(fun() ->
                receive
                after id(-1) -> ok
                end
            end),
            error_of(fun() ->
                receive
                after id(1.0) -> ok
                end
            end),
            error_of(fun() ->
                receive
                after id(4294967296) -> ok
                end
            end),
            error_of(fun() ->
                receive
                    nothing -> nothing
                after id(4294967295) - 4294967295 -> zero
                end
            end)
        ]
    ]).
