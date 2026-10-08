-module(wakeups).
-export([main/1]).

% Stress of wakeups across scheduler workers: every round races message arrivals against receive timeouts, and
% exit signals and 'DOWN' messages against processes running elsewhere. A lost wakeup hangs the program; the output
% only counts what must always happen.

% Rounds of each stress; each round races anew.
-define(ROUNDS, 4).

% Wait Milliseconds.
pause(Milliseconds) ->
    receive
    after Milliseconds -> ok
    end.

% Spin forever without waiting, until an exit signal ends the process.
spin() -> spin().

% Receive Total messages, each time waiting with a short timeout (0, 1 or 2 ms) that often expires first.
receiver(Parent, Total) -> receiver(Parent, Total, 0, 0).

receiver(Parent, Total, Total, _) ->
    Parent ! {done, self()};
receiver(Parent, Total, Got, Waits) ->
    receive
        {message, _} -> receiver(Parent, Total, Got + 1, Waits + 1)
    after Waits rem 3 -> receiver(Parent, Total, Got, Waits + 1)
    end.

% Send N messages to To, pausing 0 or 1 ms between them.
sender(_, 0) ->
    ok;
sender(To, N) ->
    To ! {message, N},
    pause(N rem 2),
    sender(To, N - 1).

% Pairs of receivers and senders; every receiver gets all its messages, whichever timeouts expire.
arrivals(Pairs, Total) ->
    Self = self(),
    Receivers = [spawn(fun() -> receiver(Self, Total) end) || _ <- lists:seq(1, Pairs)],
    [spawn(fun() -> sender(Receiver, Total) end) || Receiver <- Receivers],
    [
        receive
            {done, Receiver} -> ok
        end
     || Receiver <- Receivers
    ],
    length(Receivers).

% A chain of N linked spinning processes below the caller; each reports its pid to Parent first.
member(Parent, 0) ->
    Parent ! {member, self()},
    spin();
member(Parent, N) ->
    spawn_link(fun() -> member(Parent, N - 1) end),
    Parent ! {member, self()},
    spin().

% Kill the head of a chain of spinning processes: the exit signal runs down the links while the members run on other
% workers; every member's monitor reports the reason and the trapping caller gets the head's 'EXIT'.
chain(Length) ->
    Self = self(),
    Head = spawn_link(fun() -> member(Self, Length - 1) end),
    Members = [
        receive
            {member, Pid} -> Pid
        end
     || _ <- lists:seq(1, Length)
    ],
    Refs = [monitor(process, Member) || Member <- Members],
    exit(Head, gone),
    Reasons = [
        receive
            {'DOWN', Ref, process, _, Reason} -> Reason
        end
     || Ref <- Refs
    ],
    receive
        {'EXIT', Head, gone} -> ok
    end,
    length([gone || gone <- Reasons]).

% Monitored processes end after 0 to 2 ms, at about the same time as their watcher wakes.
endings(Count) ->
    Refs = [
        element(2, spawn_monitor(fun() -> pause(N rem 3) end))
     || N <- lists:seq(1, Count)
    ],
    Reasons = [
        receive
            {'DOWN', Ref, process, _, Reason} -> Reason
        end
     || Ref <- Refs
    ],
    length([normal || normal <- Reasons]).

stress(N) ->
    {arrivals(6, 30), chain(16), endings(20), N}.

main(["teardown"]) ->
    % The program ends while processes spin, wait with timeouts and flood each other on other workers.
    [spawn(fun spin/0) || _ <- lists:seq(1, 4)],
    Receivers = [spawn(fun() -> receiver(self(), 1000000) end) || _ <- lists:seq(1, 4)],
    [spawn(fun() -> sender(Receiver, 1000000) end) || Receiver <- Receivers],
    pause(20),
    io:format("main ends~n");
main(["halt"]) ->
    % A halt in another process ends the program while the others spin or wait.
    [spawn(fun spin/0) || _ <- lists:seq(1, 4)],
    spawn(fun() ->
        pause(10),
        halt(3)
    end),
    pause(infinity);
main(_) ->
    process_flag(trap_exit, true),
    [io:format("~p~n", [stress(N)]) || N <- lists:seq(1, ?ROUNDS)],
    io:format("done~n").
