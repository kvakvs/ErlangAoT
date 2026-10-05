-module(recursion).
-export([main/1, ping/1]).

%% The first argument selects a scenario; each recursive result is displayed.
main(["factorial"]) ->
    show(fact(id(0))),
    show(fact(id(5))),
    show(fact(id(25))),
    show(fact_tail(id(20), 1));
main(["parity"]) ->
    show(even(id(0))),
    show(even(id(10))),
    show(odd(id(7))),
    show(even(id(3))),
    show(odd(id(0)));
main(["lists"]) ->
    Built = build(id(8)),
    show(Built),
    show(sum(Built)),
    show(reverse(Built, [])),
    show(len(id("recursion"))),
    show(zip(id([a, b, c]), id([1, 2, 3])));
main(["remote"]) ->
    show(ping(id(6))),
    show(ping(id(7))),
    show(walk:depth(id({node, {node, {node, leaf}}})));
main(["errors"]) ->
    show(
        try
            countdown(id(4))
        catch
            error:Reason -> {caught, Reason}
        end
    ),
    show(
        try
            fact(id(-1))
        catch
            error:Clause -> {caught, Clause}
        end
    );
main(["uncaught"]) ->
    show(countdown(id(3))).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% Self recursion through a body call, including bignum results.
fact(0) -> 1;
fact(N) when N > 0 -> N * fact(N - 1).

%% Self recursion in tail position with an accumulator.
fact_tail(0, Acc) -> Acc;
fact_tail(N, Acc) -> fact_tail(N - 1, N * Acc).

%% Mutual recursion between two local functions.
even(0) -> true;
even(N) -> odd(N - 1).

odd(0) -> false;
odd(N) -> even(N - 1).

build(0) -> [];
build(N) -> [N | build(N - 1)].

sum([]) -> 0;
sum([H | T]) -> H + sum(T).

reverse([], Acc) -> Acc;
reverse([H | T], Acc) -> reverse(T, [H | Acc]).

len([]) -> 0;
len([_ | T]) -> 1 + len(T).

zip([], []) -> [];
zip([X | Xs], [Y | Ys]) -> [{X, Y} | zip(Xs, Ys)].

%% Cross-module recursion: ping and walk:pong call each other.
ping(0) -> {done, ping};
ping(N) -> walk:pong(N - 1).

%% An error raised at the bottom of a recursion unwinds every frame.
countdown(0) -> error(bottom);
countdown(N) -> {N, countdown(N - 1)}.
