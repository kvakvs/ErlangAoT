-module(types).
-export([f/1]).
-export_type([all/0]).

-record(r, {a :: integer()}).
-record #nr{x :: atom()}.

-type all() ::
    any()
    | none()
    | atom
    | 42
    | -1
    | 1..10
    | $c
    | 1 bsl 4
    | integer()
    | list(integer())
    | [atom()]
    | [atom(), ...]
    | []
    | {}
    | tuple()
    | {a, b}
    | map()
    | #{}
    | #{a => b, c := d}
    | #r{}
    | #r{a :: 1}
    | #nr{}
    | #types:nr{x :: ok}
    | binary()
    | <<>>
    | <<_:8>>
    | <<_:_*4>>
    | <<_:8, _:_*4>>
    | fun()
    | fun((...) -> ok)
    | fun(() -> ok)
    | fun((a, b) -> c)
    | erlang:timestamp()
    | local(atom())
    | (a | b)
    | Var :: term().
-type local(T) :: {T}.

-spec f
    (A) -> B when A :: integer(), B :: atom();
    (atom()) -> integer().
f(_) ->
    ok.

-spec types:g(integer()) -> integer().
-callback cb(Arg :: term()) -> ok | {error, Reason :: term()}.
