-module(pat_key_alias_bound).
-export([f/2]).
f(K, M) ->
    ({K, _} = #{K := X}) = M,
    X.
