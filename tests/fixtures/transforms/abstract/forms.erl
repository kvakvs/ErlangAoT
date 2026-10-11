-module(forms).
-moduledoc "Module documentation.".
-export([f/1, g/0, h/0]).
-export_type([t/0, p/1]).
-import(lists, [map/2, foldl/3]).
-compile([nowarn_unused_vars]).
-compile({inline, [g/0]}).
-behaviour(gen_server).
-vsn("1.0").
-my_attr({tuple, [1, 2.5, "str", <<1, 2>>], #{key => value}, -3}).

-include("forms.hrl").

-record(rec, {a = 1 :: integer(), b, c :: atom() | undefined}).
-record #native{x = 0, y :: binary()}.

-type t() :: integer() | atom().
-type p(T) :: [T] | {T, T}.
-opaque o() :: #{atom() => term()}.
-nominal n() :: non_neg_integer().

-callback init(term()) -> {ok, term()}.

-spec f(T) -> T when T :: integer().
-doc "Function documentation.".
f(X) ->
    X.

-spec g() -> ok.
g() ->
    ok.

-file("other.erl", 100).
h() ->
    ?VALUE.
