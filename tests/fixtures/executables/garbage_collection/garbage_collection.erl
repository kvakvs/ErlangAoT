-module(garbage_collection).
-export([main/1]).

%% Each run allocates more than the 64 MiB process budget (8 Mi words of 64
%% bits) while its live set stays small, so it only finishes when generated
%% code collects.

%% 200 characters: one string literal is one 400-word list per evaluation.
-define(TEXT,
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "012345678901234567890123456789012345678901234567890123456789"
).

%% The first argument selects a scenario.
main(["churn"]) ->
    show(churn(id(25000), none));
main(["binaries"]) ->
    show(binaries(id(9000), <<0:65536>>, 0));
main(["comprehension"]) ->
    show(len([X || X <- seq(id(25000)), keep(X)]));
main(["deep"]) ->
    List = deep(id(20000)),
    show(len(List)),
    show(sum(List)),
    show(valid(List));
main(["payload"]) ->
    Big =
        try
            fail(id(2000))
        catch
            error:{boom, Payload} -> Payload
        end,
    Kept = churn(id(25000), Big),
    show(len(element(2, Kept))),
    show(sum(element(2, Kept))).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% A tail loop that builds a 400-word string per step and keeps only its first
%% character and the value carried in.
churn(0, Keep) ->
    Keep;
churn(N, Keep) ->
    [First | _] = ?TEXT,
    churn(N - 1, next(Keep, First)).

next(none, First) -> {First, none};
next({_, Kept}, First) -> {First, Kept};
next(Kept, First) -> {First, Kept}.

%% A tail loop that builds an 8196-byte off-heap binary per step.
binaries(N, Base, Total) -> binaries(N, Base, Base, Total).

binaries(0, _, Last, Total) ->
    {Total, size_of(Last)};
binaries(N, Base, _, Total) ->
    binaries(N - 1, Base, <<N:32, Base/binary>>, Total + 1).

size_of(Binary) when byte_size(Binary) =:= 8196 -> 8196.

%% A comprehension whose filter allocates and drops a string per element.
keep(X) ->
    [First | _] = ?TEXT,
    First =:= $0 andalso X rem 3 =:= 0.

seq(N) -> seq(N, []).

seq(0, Acc) -> Acc;
seq(N, Acc) -> seq(N - 1, [N | Acc]).

%% Body recursion: every frame keeps a nested term across the recursive call
%% while the callees allocate garbage.
deep(0) ->
    [];
deep(N) ->
    Mine = {N, [N, N + 1], <<N:32>>},
    [_ | _] = ?TEXT,
    Rest = deep(N - 1),
    [Mine | Rest].

valid([]) -> true;
valid([{N, [N, M], <<N:32>>} | Rest]) when M =:= N + 1 -> valid(Rest);
valid(_) -> false.

len([]) -> 0;
len([_ | Tail]) -> 1 + len(Tail).

sum([]) -> 0;
sum([{N, _, _} | Tail]) -> N + sum(Tail);
sum([N | Tail]) -> N + sum(Tail).

%% Body recursion that allocates on the way down and raises a list payload.
fail(0) ->
    error({boom, seq(1000)});
fail(N) ->
    [_ | _] = ?TEXT,
    1 + fail(N - 1).
