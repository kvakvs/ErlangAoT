-module(service_answer).
-export([head/1, body/1, head_mismatch/1, skipped/1, reached/1, strict/1, fallback/1, match_once/1, match_stop/1]).
head(X) when is_integer(X); true -> ok.
body(X) -> is_integer(X).
head_mismatch(0) when is_integer(0); true -> ok.
skipped(X) -> true orelse is_integer(X).
reached(X) when (is_integer(X) orelse true); true -> ok.
strict(X) -> true or is_integer(X).
fallback(X) when is_integer(X) -> ok; fallback(_) -> recovered.
match_once(X) -> Y = is_integer(X), Y.
match_stop(X) -> false = is_integer(X), hd([]).
