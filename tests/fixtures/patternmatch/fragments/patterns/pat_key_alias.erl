-module(pat_key_alias).
-export([f/1]).
f({K, _} = #{K := X}) -> X.
