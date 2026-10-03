-module(answer).
-export([
    repeat/2,
    aliases/1,
    literal/1,
    atom/1,
    nil/1,
    tuple/1,
    wildcard/2,
    constant/1,
    low32/1,
    high32/1,
    low64/1,
    high64/1,
    wrong_spec/2
]).
repeat(X, X) -> X.
aliases((First = Second) = (Third = First)) ->
    _ = Second,
    Third.
literal($v = Selected) ->
    118 = Selected,
    ok.
atom(ok = X) -> X.
nil(Empty = []) ->
    "" = Empty,
    Empty.
tuple({} = X) -> X.
wildcard(_Name, _) -> _Name.
constant(1 + 2 * 3) -> 7.
low32(-134217728) -> low.
high32(134217727) -> high.
low64(-576460752303423488) -> low.
high64(576460752303423487) -> high.
-spec wrong_spec(integer(), integer()) -> integer().
wrong_spec(X, X) -> X.
