-module(answer).
-export([
    default/0,
    empty/0,
    construct/2,
    wild/1,
    wild_alloc/1,
    ordered/0,
    ordered_bind/0,
    default_calls/0,
    make_default/0,
    head/1,
    repeat/1,
    head_order/1,
    alias/1,
    body/1,
    access/1,
    guard_access/1,
    guard_alternative/1,
    guard_construct/1,
    test/1,
    dynamic2/2,
    dynamic3/3,
    guard_test/1,
    legacy/1,
    guard3/1,
    guard_zero/1,
    guard_big/1,
    index/1,
    key/1,
    eval_once/1,
    nested/1,
    nested_defaults/0,
    retained/1,
    wrong_spec/1,
    id/1
]).
-include("records.hrl").
default() -> #r{}.
empty() -> {#empty{}, #r.a, #r.b, #r.c}.
construct(A, B) -> #r{b = B, a = A}.
wild(X) -> #r{a = 1, _ = X}.
wild_alloc(X) -> #r{_ = {id(X), [X]}}.
ordered() -> #r{b = (1 div 0), a = ((1)#r.a)}.
ordered_bind() -> #r{b = (X = 2), a = (X = 1)}.
default_calls() -> #d{}.
make_default() -> {default, <<1:3>>, #{a => 1}}.
head(#r{b = X}) -> X;
head(_) -> no.
repeat(#r{_ = X}) -> X;
repeat(_) -> no.
head_order(#r{b = X, a = X}) -> X;
head_order(_) -> no.
alias(#r{a = A, b = B} = R) -> {R, A, B};
alias(_) -> no.
body(X) ->
    #r{b = B} = X,
    B.
access(X) -> X#r.b.
guard_access(X) when X#r.a == 1 -> yes;
guard_access(_) -> no.
guard_alternative(X) when X#r.a == 1; is_atom(X) -> yes;
guard_alternative(_) -> no.
guard_construct(X) when (#r{b = X})#r.b =:= X -> yes;
guard_construct(_) -> no.
test(X) ->
    {is_record(X, r), erlang:is_record(X, empty), is_record(X, r, 4), is_record(X, r, native)}.
dynamic2(X, T) -> erlang:is_record(X, T).
dynamic3(X, T, N) -> erlang:is_record(X, T, N).
guard_test(X) when is_record(X, r) -> yes;
guard_test(_) -> no.
legacy(X) when record(X, r) -> yes;
legacy(_) -> no.
guard3(X) when erlang:is_record(X, r, 4) -> yes;
guard3(_) -> no.
guard_zero(X) when is_record(X, r, 0) -> yes;
guard_zero(_) -> no.
guard_big(X) when is_record(X, r, 10000) -> yes;
guard_big(_) -> no.
index(#r.b) -> yes;
index(_) -> no.
key(M) ->
    #{#r.b := V} = M,
    V.
eval_once(X) -> {is_record(id(X), r), (id(X))#r.a}.
nested(N) -> N#nrec2.nrec1#nrec1.nrec0#nrec0.name.
nested_defaults() ->
    N = #nrec2{},
    {N, nested(N)}.
retained(X) ->
    R = #r{b = {X, <<1:3>>}},
    default_calls(),
    R.
-spec wrong_spec(integer()) -> integer().
wrong_spec(X) -> X#r.b.
id(Value) ->
    Saved = Value,
    Saved.
