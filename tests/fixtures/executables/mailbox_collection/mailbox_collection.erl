-module(mailbox_collection).
-export([main/1]).

% Collections of processes that hold many messages or wait in a receive. A heap starts at 233 words, so every few
% messages delivered into it leave a fragment that the next safepoint of its process collects.

% A message of about 30 heap words, shared subterms and a binary large enough to live off the heap (> 64 bytes).
message(I) ->
    Shared = [I, I + 1, I + 2],
    {item, I, Shared, {Shared, Shared}, <<I:32, 0:640>>}.

% Whether Message is message(I), intact.
intact(I, {item, I, [I, J, K] = Shared, {Shared, Shared}, <<I:32, 0:640>>}) ->
    J =:= I + 1 andalso K =:= I + 2;
intact(_, _) ->
    false.

% Wait for release, skipping every other message, then take all of them in order and report how many were intact.
hoarder(Parent, Count) ->
    receive
        release -> ok
    end,
    Parent ! {checked, self(), check(1, Count, 0)}.

check(I, Count, Good) when I > Count -> Good;
check(I, Count, Good) ->
    receive
        Message -> check(I + 1, Count, Good + bool(intact(I, Message)))
    end.

sum([], Sum) -> Sum;
sum([X | Rest], Sum) -> sum(Rest, Sum + X).

bool(true) -> 1;
bool(false) -> 0.

% Garbage made between sends, so the sender collects too.
churn(0, Acc) -> length(Acc);
churn(N, Acc) -> churn(N - 1, [lists:seq(1, 8) | Acc]).

fill(_, I, Count) when I > Count -> ok;
fill(Pid, I, Count) ->
    Pid ! message(I),
    churn(4, []),
    fill(Pid, I + 1, Count).

% A process waiting with a timeout receives messages it does not match: the timeout is kept across collections and
% the messages stay in order behind the cursor.
waiter(Parent) ->
    Result =
        receive
            never -> never
        after 300 -> timeout
        end,
    Parent ! {waited, Result, check(1, 400, 0)}.

% A process in the middle of a deep recursion holds live data in its frames while messages arrive.
deep(Parent, 0, Acc) ->
    receive
        go -> Parent ! {deep, sum(Acc, 0), check(1, 300, 0)}
    end;
deep(Parent, N, Acc) ->
    [deep(Parent, N - 1, [N | Acc])].

% Take Count messages one at a time, acknowledging each: the consumer waits between messages, so only collections
% of a waiting process keep its heap small while all messages together far exceed it.
consumer(Parent, Count) ->
    Parent ! {consumed, check_acked(Parent, 1, Count, 0)}.

check_acked(_, I, Count, Good) when I > Count -> Good;
check_acked(Parent, I, Count, Good) ->
    receive
        Message ->
            Parent ! ack,
            check_acked(Parent, I + 1, Count, Good + bool(intact(I, Message)))
    end.

produce(_, I, Count) when I > Count -> ok;
produce(Pid, I, Count) ->
    Pid ! message(I),
    receive
        ack -> produce(Pid, I + 1, Count)
    end.

main(["bounded"]) ->
    Self = self(),
    Consumer = spawn(fun() -> consumer(Self, 3000) end),
    produce(Consumer, 1, 3000),
    receive
        {consumed, Good} -> io:format("consumed ~p~n", [Good])
    end;
main(_) ->
    Self = self(),
    % One hoarder with a large mailbox.
    Hoarder = spawn(fun() -> hoarder(Self, 2000) end),
    fill(Hoarder, 1, 2000),
    Hoarder ! release,
    receive
        {checked, Hoarder, Good} -> io:format("hoarder ~p~n", [Good])
    end,
    % Many hoarders filled in turns.
    Many = [spawn(fun() -> hoarder(Self, 100) end) || _ <- lists:seq(1, 40)],
    [fill(Pid, 1, 100) || Pid <- Many],
    [Pid ! release || Pid <- Many],
    Totals = [
        receive
            {checked, Pid, Count} -> Count
        end
     || Pid <- Many
    ],
    io:format("many ~w~n", [[T || T <- Totals, T =/= 100]]),
    % A waiting receive with a timeout.
    Waiter = spawn(fun() -> waiter(Self) end),
    fill(Waiter, 1, 400),
    receive
        {waited, Result, Checked} -> io:format("waiter ~p ~p~n", [Result, Checked])
    end,
    % A process suspended deep in a recursion.
    Deep = spawn(fun() -> deep(Self, 3000, []) end),
    fill(Deep, 1, 300),
    Deep ! go,
    receive
        {deep, Sum, Intact} -> io:format("deep ~p ~p~n", [Sum, Intact])
    end.
