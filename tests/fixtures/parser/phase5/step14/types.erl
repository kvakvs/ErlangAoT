-module(types).
-type scalar(A) :: X :: (A | ok | 1..(3+4*5) | $z | -7).
-type collections() :: {[], [integer()], [atom(),...], #{atom() => [integer()], fixed := binary()}, #rec{field :: A}}.
-type applications() :: {tuple(), map(), list(), list(atom()), custom(A), other:t(integer()), <<>>, <<_:8>>, <<_:_*8>>, <<_:4,_:_*8>>}.
-opaque callable(A) :: {fun(), fun((A, atom()) -> [A]), fun((...) -> any()), fun(() -> ok)}.
-nominal (tag(A) :: {tag, A}).
-record(r, {plain, typed :: integer(), value = init() :: #{atom() => integer()}, last}).
-record #Point{value = 0 :: integer(), label :: binary()}.
-type native() :: #other:Point{value :: 1..9}.
f() -> #r{}.
