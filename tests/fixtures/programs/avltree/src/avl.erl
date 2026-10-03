%% Persistent AVL tree keyed by ordered terms, stored as tuple records.
-module(avl).
-export([new/0, insert/3, lookup/2, delete/2, fold/3, map/2, to_list/1]).
-export([count/1, height/1, check/1, fields/0]).

-record(node, {key, value, left = nil, right = nil, height = 1}).

%% Returns the empty tree.
new() -> nil.

%% Returns the cached subtree height.
height(nil) -> 0;
height(#node{height = H}) -> H.

%% Lists the record field names, resolved at compile time.
fields() -> record_info(fields, node).

node(K, V, L, R) ->
    #node{key = K, value = V, left = L, right = R, height = 1 + max(height(L), height(R))}.

%% Inserts or replaces a key, rebalancing on the way back up.
insert(K, V, nil) ->
    node(K, V, nil, nil);
insert(K, V, #node{key = NK, left = L} = N) when K < NK ->
    rebalance(N#node{left = insert(K, V, L)});
insert(K, V, #node{key = NK, right = R} = N) when K > NK ->
    rebalance(N#node{right = insert(K, V, R)});
insert(_, V, N) ->
    N#node{value = V}.

%% Restores the height invariant at one node.
rebalance(#node{key = K, value = V, left = L, right = R}) ->
    case height(L) - height(R) of
        2 -> rotate_right(K, V, L, R);
        -2 -> rotate_left(K, V, L, R);
        _ -> node(K, V, L, R)
    end.

rotate_right(K, V, #node{key = LK, value = LV, left = LL, right = LR}, R) ->
    Outer = height(LL),
    Inner = height(LR),
    if
        Outer >= Inner ->
            node(LK, LV, LL, node(K, V, LR, R));
        true ->
            #node{key = MK, value = MV, left = ML, right = MR} = LR,
            node(MK, MV, node(LK, LV, LL, ML), node(K, V, MR, R))
    end.

rotate_left(K, V, L, #node{key = RK, value = RV, left = RL, right = RR}) ->
    Outer = height(RR),
    Inner = height(RL),
    if
        Outer >= Inner ->
            node(RK, RV, node(K, V, L, RL), RR);
        true ->
            #node{key = MK, value = MV, left = ML, right = MR} = RL,
            node(MK, MV, node(K, V, L, ML), node(RK, RV, MR, RR))
    end.

%% Finds a key's value as {ok, Value} or error.
lookup(_, nil) -> error;
lookup(K, #node{key = K, value = V}) -> {ok, V};
lookup(K, #node{key = NK, left = L}) when K < NK -> lookup(K, L);
lookup(K, #node{right = R}) -> lookup(K, R).

%% Removes a key if present.
delete(_, nil) ->
    nil;
delete(K, #node{key = NK, left = L} = N) when K < NK ->
    rebalance(N#node{left = delete(K, L)});
delete(K, #node{key = NK, right = R} = N) when K > NK ->
    rebalance(N#node{right = delete(K, R)});
delete(_, #node{left = nil, right = R}) ->
    R;
delete(_, #node{left = L, right = nil}) ->
    L;
delete(_, #node{left = L, right = R}) ->
    {MK, MV, Rest} = take_min(R),
    rebalance(node(MK, MV, L, Rest)).

take_min(#node{key = K, value = V, left = nil, right = R}) ->
    {K, V, R};
take_min(#node{left = L} = N) ->
    {K, V, Rest} = take_min(L),
    {K, V, rebalance(N#node{left = Rest})}.

%% Folds F(Key, Value, Acc) over keys in ascending order.
fold(_, Acc, nil) ->
    Acc;
fold(F, Acc, #node{key = K, value = V, left = L, right = R}) ->
    fold(F, F(K, V, fold(F, Acc, L)), R).

%% Applies F to every value, keeping the shape.
map(_, nil) ->
    nil;
map(F, #node{value = V, left = L, right = R} = N) ->
    N#node{value = F(V), left = map(F, L), right = map(F, R)}.

%% Returns ascending {Key, Value} pairs.
to_list(T) -> lists:reverse(fold(fun(K, V, Acc) -> [{K, V} | Acc] end, [], T)).

%% Counts entries.
count(T) -> fold(fun(_, _, N) -> N + 1 end, 0, T).

%% Verifies ordering, balance and cached heights; returns ok or {error, Reason}.
check(T) ->
    try verify(T, none, none) of
        _ -> ok
    catch
        throw:Reason -> {error, Reason}
    end.

verify(nil, _, _) ->
    0;
verify(#node{key = K, left = L, right = R, height = H}, Low, High) ->
    in_range(K, Low, High) orelse throw({unordered, K}),
    HL = verify(L, Low, {K}),
    HR = verify(R, {K}, High),
    abs(HL - HR) =< 1 orelse throw({unbalanced, K}),
    H =:= 1 + max(HL, HR) orelse throw({stale_height, K}),
    H.

in_range(K, Low, High) -> above(K, Low) andalso below(K, High).

above(_, none) -> true;
above(K, {Low}) -> K > Low.

below(_, none) -> true;
below(K, {High}) -> K < High.
