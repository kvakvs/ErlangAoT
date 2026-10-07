%% Project-owned subset of the lists module (plan 11 step 39): original
%% implementations whose results and error reasons follow OTP's documented
%% behavior. Compiled from source into every program that calls it.
-module(lists).
-export([
    append/1,
    append/2,
    filter/2,
    foldl/3,
    foldr/3,
    keyfind/3,
    map/2,
    member/2,
    nth/2,
    reverse/1,
    reverse/2,
    seq/2,
    seq/3,
    sort/1
]).

%% The concatenation of a list of lists.
append([Last]) -> Last;
append([First | Rest]) -> First ++ append(Rest);
append([]) -> [].

%% Two lists joined: List1 ++ List2.
append(List1, List2) -> List1 ++ List2.

%% The elements for which Pred returns true; anything else from Pred is an error.
filter(Pred, List) when is_function(Pred, 1) ->
    [Element || Element <- List, Pred(Element)].

%% Fold from the first element: Fun(Element, Acc) returns the next Acc. A non-list
%% is a case_clause error, an improper tail a function_clause error, as in OTP.
foldl(Fun, Acc, List) when is_function(Fun, 2) ->
    case List of
        [Element | Rest] -> fold_left(Fun, Fun(Element, Acc), Rest);
        [] -> Acc
    end.

fold_left(Fun, Acc, [Element | Rest]) -> fold_left(Fun, Fun(Element, Acc), Rest);
fold_left(_, Acc, []) -> Acc.

%% Fold from the last element.
foldr(Fun, Acc, [Element | Rest]) when is_function(Fun, 2) ->
    Fun(Element, foldr(Fun, Acc, Rest));
foldr(Fun, Acc, []) when is_function(Fun, 2) ->
    Acc.

%% The first tuple whose Nth element compares equal (==) to Key, or false.
keyfind(Key, N, List) when is_integer(N), N >= 1 ->
    keyfind_from(Key, N, List);
keyfind(_, _, _) ->
    erlang:error(badarg).

keyfind_from(Key, N, [Tuple | _]) when
    is_tuple(Tuple), tuple_size(Tuple) >= N, element(N, Tuple) == Key
->
    Tuple;
keyfind_from(Key, N, [_ | Rest]) ->
    keyfind_from(Key, N, Rest);
keyfind_from(_, _, []) ->
    false;
keyfind_from(_, _, _) ->
    erlang:error(badarg).

%% Fun applied to every element, in order. A non-list is a case_clause error, an
%% improper tail a function_clause error, as in OTP.
map(Fun, List) when is_function(Fun, 1) ->
    case List of
        [Element | Rest] -> [Fun(Element) | map_rest(Fun, Rest)];
        [] -> []
    end.

map_rest(Fun, [Element | Rest]) -> [Fun(Element) | map_rest(Fun, Rest)];
map_rest(_, []) -> [].

%% Whether an element matches (=:=) Element.
member(Element, [Element | _]) -> true;
member(Element, [_ | Rest]) -> member(Element, Rest);
member(_, []) -> false;
member(_, _) -> erlang:error(badarg).

%% The Nth element, counting from 1.
nth(1, [Element | _]) -> Element;
nth(N, [_ | _] = List) when is_integer(N), N > 1 -> nth_from(N, List).

nth_from(1, [Element | _]) -> Element;
nth_from(N, [_ | Rest]) -> nth_from(N - 1, Rest).

%% The elements in reverse order.
reverse([] = List) -> List;
reverse([_] = List) -> List;
reverse([A, B]) -> [B, A];
reverse([A, B | Rest]) -> reverse(Rest, [B, A]).

%% The elements of List in reverse order, followed by Tail.
reverse([Element | Rest], Tail) -> reverse(Rest, [Element | Tail]);
reverse([], Tail) -> Tail;
reverse(_, _) -> erlang:error(badarg).

%% The integers from First to Last.
seq(First, Last) when is_integer(First), is_integer(Last), First - 1 =< Last ->
    count_down(Last, First, 1, []).

%% The integers from First stepping by Increment while not passing Last.
seq(First, Last, Increment) when
    is_integer(First),
    is_integer(Last),
    is_integer(Increment),
    (Increment > 0 andalso First - Increment =< Last) orelse
        (Increment < 0 andalso First - Increment >= Last)
->
    Count = (Last - First + Increment) div Increment,
    count_down(First + Increment * (Count - 1), First, Increment, []);
seq(Same, Same, 0) when is_integer(Same) ->
    [Same];
seq(_, _, _) ->
    erlang:error(badarg).

%% Prepend From, From - Step, ... while it has not passed To.
count_down(From, To, Step, Acc) when Step > 0, From >= To; Step < 0, From =< To ->
    count_down(From - Step, To, Step, [From | Acc]);
count_down(_, _, _, Acc) ->
    Acc.

%% The elements in ascending term order; equal elements keep their order.
sort([_, _ | _] = List) ->
    merge_all(runs(List, []));
sort([_] = List) ->
    List;
sort([] = List) ->
    List.

%% Split into one-element runs, in order.
runs([Element | Rest], Acc) -> runs(Rest, [[Element] | Acc]);
runs([], Acc) -> reverse(Acc, []).

%% Merge runs pairwise until one remains.
merge_all([Run]) -> Run;
merge_all(Runs) -> merge_all(merge_pairs(Runs, [])).

merge_pairs([A, B | Rest], Acc) -> merge_pairs(Rest, [merge(A, B, []) | Acc]);
merge_pairs([A], Acc) -> reverse(Acc, [A]);
merge_pairs([], Acc) -> reverse(Acc, []).

%% Merge two sorted runs; on ties the left run comes first.
merge([X | Xs] = Left, [Y | Ys] = Right, Acc) ->
    case X =< Y of
        true -> merge(Xs, Right, [X | Acc]);
        false -> merge(Left, Ys, [Y | Acc])
    end;
merge([], Right, Acc) ->
    reverse(Acc, Right);
merge(Left, [], Acc) ->
    reverse(Acc, Left).
