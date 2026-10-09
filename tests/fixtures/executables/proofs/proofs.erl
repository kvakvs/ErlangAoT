-module(proofs).
-export([main/1, scale/2]).

% Operations whose checks inferred facts remove at -O2 (plan 11 step 59). Local functions take the facts of their
% callers' arguments; the exported scale/2 keeps its checks. Results must not depend on the optimization level.

% Tuple access: both points are pairs of small integers, so the shape test, the element reads and the arithmetic
% are inline.
norm({X, Y}) -> X * X + Y * Y.

% A list loop over a proper list of small integers: cons cells are read inline; the sum keeps its overflow check.
sum([H | T], Acc) -> sum(T, Acc + H);
sum([], Acc) -> Acc.

% A comprehension over a proven list, with proven small products.
squares(L) -> [X * X || X <- L].

% A body match on a proven pair, and a case on a proven list.
swap(P) ->
    {A, B} = P,
    {B, A}.

first(L) ->
    case L of
        [H | _] -> H;
        [] -> none
    end.

% The largest small integer plus one leaves the immediate range: the proven operand skips its tag test, but the
% result still overflows into a bignum through the generic fallback.
edge(X) -> X + 1.

% Callers outside the module are unknown, so these checks stay generic.
scale(X, F) -> X * F.

main(_) ->
    erlang:display([norm(P) || P <- [{1, 2}, {3, 4}, {5, 12}]]),
    erlang:display(sum([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 0)),
    erlang:display(squares([1, 2, 3])),
    erlang:display(swap({a, 1})),
    erlang:display({first([7, 8]), first([])}),
    erlang:display({edge(576460752303423487), edge(134217727), edge(-576460752303423488)}),
    erlang:display({scale(3, 4), scale(1.5, 2), scale(1 bsl 62, 4)}).
