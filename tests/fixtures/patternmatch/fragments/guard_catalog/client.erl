-module(client).
-export([range/3, retry/1]).
range(X, L, U) -> answer:range_alt(X, L, U).
retry(X) -> answer:construct(X).
