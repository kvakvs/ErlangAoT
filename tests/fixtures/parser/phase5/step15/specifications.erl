-module(specifications).
-spec f(A, B) -> {A,B} when A :: integer(), B :: atom().
-spec (over(integer()) -> atom(); (atom()) -> integer()).
-spec other:remote(A) -> A when is_subtype(A, [integer()]).
-callback (handle(A, B) -> {ok,A} when A :: any(), is_subtype(B, #{atom() => term()})).
-callback other:qualified() -> ok.
-spec annotations(Arg :: integer()) -> Result :: integer().
-spec unusual() -> ok; (integer()) -> atom(); (...) -> any().
-spec legacy_group(A) -> A when is_subtype((A), {tag,integer()}).
f(A,B) -> {A,B}.
