-module(dynpeer).
-export([double/1, triple/1, adder/1, depth/1, visible/1, show/2]).

double(X) -> 2 * X.

triple(X) -> 3 * X.

adder(N) -> fun(X) -> X + N end.

%% Non-tail recursion through a dynamic call.
depth(0) ->
    0;
depth(N) ->
    M = module(),
    1 + M:depth(N - 1).

module() -> dynpeer.

visible(X) -> hidden(X).

hidden(X) -> {hidden, X}.

%% Display a tag and return the value: shows evaluation order.
show(Tag, Value) ->
    erlang:display(Tag),
    Value.
