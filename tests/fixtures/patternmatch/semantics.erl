%% Authored semantic seeds, not extracted Common Test helpers.
-module(semantics).
-export([wild/2, named/2, repeated/2, compound/1, sequence/1, chained/1,
         alternatives/1, reached/1, skipped/1, strict/1, nonboolean/1,
         qualified/1, legacy/1, range/1, containers/1, map_key/2, bits/1,
         record_match/1, constant/1, prefix/1, legacy_float/1, converted_float/1]).
-record(r, {value}).

wild(_, _) -> 1.
named(_Name, _Name) -> 1;
named(_, _) -> 2.
repeated(X, X) -> X.
compound({X, X} = Pair) -> Pair.
sequence(X) -> Y = X, 7 = Y, Y.
chained(X) -> A = B = X, {A, B}.
alternatives(X) when hd(X) =:= 1; X =:= 7 -> 1;
alternatives(_) -> 2.
reached(X) when (hd(X) =:= 1) orelse X =:= 7 -> 1;
reached(_) -> 2.
skipped(X) when X =:= 7 orelse hd(X) =:= 1 -> 1;
skipped(_) -> 2.
strict(X) when (X =:= 7) or (hd(X) =:= 1) -> 1;
strict(_) -> 2.
nonboolean(X) when true andalso X -> 1;
nonboolean(_) -> 2.
qualified(X) when erlang:is_integer(X), erlang:'>'(X, 0) -> 1;
qualified(_) -> 2.
legacy(X) when integer(X) -> 1;
legacy(_) -> 2.
legacy_float(X) when float(X) -> 1;
legacy_float(_) -> 2.
converted_float(X) when erlang:float(X) =:= 7.0 -> 1;
converted_float(_) -> 2.
range(X) when is_integer(X, 1, 3) -> 1;
range(_) -> 2.
containers({[H | T], #{key := V}}) -> {H, T, V}.
map_key(K, M) -> #{K := V} = M, V.
bits(<<N:8, V:N, Rest/bitstring>>) -> {V, Rest}.
record_match(#r{value = V}) -> V.
constant(1 + 2) -> 1;
constant(_) -> 2.
prefix("ab" ++ Tail) -> Tail.
