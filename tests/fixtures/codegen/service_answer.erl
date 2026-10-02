-module(service_answer).
-export([head/1, body/1, head_mismatch/1]).
head(X) when is_integer(X) -> ok.
body(X) -> is_integer(X).
head_mismatch(0) when is_integer(0) -> ok.
