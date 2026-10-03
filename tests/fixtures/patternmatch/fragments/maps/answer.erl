-module(answer).
-export([
    map_is_size/2,
    check_map_value/3,
    map_get_head/1,
    empty/0,
    literal/0,
    construct/2,
    associate/3,
    update/3,
    mixed/3,
    exact_first/3,
    get/2,
    contains/2,
    size_value/1,
    classify/1,
    head/1,
    empty_head/1,
    extra/1,
    duplicate/1,
    dup_keys/1,
    dup_keys_inner/1,
    contradictory/1,
    numeric_keys/1,
    nested/1,
    compound_key/2,
    bound_key/2,
    key_call/3,
    key_arithmetic/3,
    equal_keys/3,
    key_fail_head/1,
    key_constant/1,
    body/1,
    repeat/2,
    compare/2,
    guard_construct/1,
    guard_update/2,
    guard_get/2,
    ordered/1,
    independent/1,
    wrong_spec/1,
    id/1
]).
map_is_size(Candidate, Count) when Count =:= map_size(Candidate) -> true;
map_is_size(_, _) -> false.
check_map_value(Candidate, Key, Expected) when Expected =:= map_get(Key, Candidate) -> true;
check_map_value(_, _, _) -> false.
map_get_head(Candidate) when 1 =:= map_get(a, Candidate) -> true;
map_get_head(_) -> false.
empty() -> #{}.
literal() ->
    #{a => 1, b => 2, a => 3, 1 => integer, 1.0 => float, 0.0 => positive, -0.0 => negative}.
construct(K, V) -> #{K => V, {key, K} => {V, [V]}, a => K}.
associate(M, K, V) -> M#{K => V}.
update(M, K, V) -> M#{K := V}.
mixed(M, K, V) -> M#{K => first, K := V}.
exact_first(M, K, V) -> M#{K := first, K => V}.
get(K, M) -> map_get(K, M).
contains(K, M) -> is_map_key(K, M).
size_value(M) -> map_size(M).
classify(M) -> {is_map(M), is_tuple(M), is_number(M)}.
head(#{a := V}) -> V;
head(_) -> missing.
empty_head(#{}) -> map;
empty_head(_) -> other.
extra(#{a := 1, b := V}) -> V;
extra(_) -> no.
duplicate(#{a := X, a := X}) -> X;
duplicate(_) -> no.
dup_keys(Candidate = #{'__struct__' := Marker}) ->
    _ = Marker,
    dup_keys_inner(Candidate);
dup_keys(Other) ->
    Other.
dup_keys_inner(#{'__struct__' := Marker}) ->
    _ = Marker,
    ok;
dup_keys_inner(Other) ->
    Other.
contradictory(#{a := 1, a := 2}) -> impossible;
contradictory(_) -> no.
numeric_keys(#{1 := X, 1.0 := Y}) -> {X, Y};
numeric_keys(_) -> no.
nested(#{a := {X, [X | T]}} = M) -> {X, T, M};
nested(_) -> no.
compound_key(K, M) ->
    #{{tag, K + 1} := V} = M,
    V.
bound_key(K, M) ->
    #{K := V} = M,
    V.
key_call(I, T, M) ->
    #{element(I, T) := V} = M,
    V.
key_arithmetic(A, B, M) ->
    #{A div B := V} = M,
    V.
equal_keys(A, B, M) ->
    #{A := X, B := X} = M,
    X.
key_fail_head(#{1 div 0 := _}) -> impossible;
key_fail_head(_) -> recovered.
key_constant(#{(1 + 2) := V}) -> V;
key_constant(_) -> no.
body(M) ->
    #{a := 1} = M,
    M.
repeat(M, M) -> same;
repeat(_, _) -> different.
compare(A, B) -> {A =:= B, A == B, A < B, A =< B, A > B, A >= B}.
guard_construct(X) when map_get(a, #{a => X}) =:= X -> X.
guard_update(M, V) when map_get(a, M#{a := V}) =:= V -> yes;
guard_update(_, _) -> no.
guard_get(K, M) when is_map_key(K, M), map_get(K, M) =:= ok -> yes;
guard_get(_, _) -> no.
ordered(M) -> M#{a := hd([])}.
independent(X) ->
    A = #{a => {X, [X]}, b => 1},
    B = #{b => 1, a => {X, [X]}},
    A = B,
    A.
-spec wrong_spec(integer()) -> integer().
wrong_spec(#{a := V}) -> V;
wrong_spec(_) -> no.
id(Value) ->
    Saved = Value,
    Saved.
