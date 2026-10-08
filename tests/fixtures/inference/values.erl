%% Type inference expectations (tests/compiler/inference/expectations.py, docs/semantic.md#inference-expectations).
%% `expect:` above a function is the signature --print-types should infer for it; `today:` is what inference finds
%% now when it falls short. Exported functions' inputs are always term(); local functions see their callers.
-module(values).
-export([
    integer/0,
    negative/0,
    float/0,
    atom/0,
    sum/0,
    product/0,
    division/0,
    comparison/0,
    conjunction/0,
    call_integer/0,
    call_local/0,
    pick/1,
    clauses/1,
    bounded/1,
    integer_or_float/1,
    scaled/1,
    same_list/0,
    mixed_list/0,
    nested_list/0,
    empty_list/0,
    string/0,
    same_tuple/0,
    mixed_tuple/0,
    nested_tuple/0,
    tagged/1,
    atom_map/0,
    integer_map/0,
    tuple_key_map/0,
    mixed_map/0,
    updated_map/1,
    returns_fun/0,
    returns_remote_fun/0,
    returns_closure/1,
    applies_fun/0,
    local_fun/0,
    binary/0,
    identity/1,
    second/2
]).

%% Simple expressions.

%% expect: integer() -> 42
integer() -> 42.

%% expect: negative() -> -7
negative() -> -7.

%% expect: float() -> float()
%% today: float() -> term()
float() -> 2.5.

%% expect: atom() -> ok
%% today: atom() -> term()
atom() -> ok.

%% expect: sum() -> 3
%% today: sum() -> term()
sum() -> 1 + 2.

%% expect: product() -> 42
%% today: product() -> term()
product() -> 6 * 7.

%% expect: division() -> float()
%% today: division() -> term()
division() -> 7 / 2.

%% expect: comparison() -> true
%% today: comparison() -> term()
comparison() -> 1 < 2.

%% expect: conjunction() -> false
%% today: conjunction() -> term()
conjunction() -> true andalso false.

%% Functions returning what other functions return.

%% expect: call_integer() -> 42
call_integer() -> integer().

%% expect: call_local() -> 4
%% today: call_local() -> term()
call_local() -> increment(3).

%% expect: increment(3) -> 4
%% today: increment(term()) -> term()
increment(X) -> X + 1.

%% Integers, integer ranges and integers or floats.

%% expect: pick(term()) -> 1 | 2 | 3
pick(X) ->
    case X of
        a -> 1;
        b -> 2;
        _ -> 3
    end.

%% expect: clauses(term()) -> 10 | 20
clauses(X) when X > 0 -> 10;
clauses(_) -> 20.

%% expect: bounded(term()) -> 1..10
%% today: bounded(term()) -> argument 1
bounded(X) when is_integer(X), X >= 1, X =< 10 -> X.

%% expect: integer_or_float(term()) -> 1 | float()
%% today: integer_or_float(term()) -> term()
integer_or_float(X) ->
    case X of
        1 -> 1;
        _ -> 2.5
    end.

%% expect: scaled(term()) -> number()
%% today: scaled(term()) -> term()
scaled(X) when is_integer(X) -> X * 2;
scaled(X) when is_float(X) -> X / 2.

%% Lists.

%% expect: same_list() -> [1 | 2 | 3, ...]
%% today: same_list() -> term()
same_list() -> [1, 2, 3].

%% expect: mixed_list() -> [1 | float() | a, ...]
%% today: mixed_list() -> term()
mixed_list() -> [1, a, 2.5].

%% expect: nested_list() -> [[1 | 2 | 3, ...], ...]
%% today: nested_list() -> term()
nested_list() -> [[1], [2, 3]].

%% expect: empty_list() -> []
%% today: empty_list() -> term()
empty_list() -> [].

%% expect: string() -> [97 | 98 | 99, ...]
%% today: string() -> term()
string() -> "abc".

%% Tuples.

%% expect: same_tuple() -> {1, 2, 3}
%% today: same_tuple() -> term()
same_tuple() -> {1, 2, 3}.

%% expect: mixed_tuple() -> {ok, 1, float()}
%% today: mixed_tuple() -> term()
mixed_tuple() -> {ok, 1, 2.5}.

%% expect: nested_tuple() -> {{a, 1}, {b, 2}}
%% today: nested_tuple() -> term()
nested_tuple() -> {{a, 1}, {b, 2}}.

%% expect: tagged(term()) -> {error, bad} | {ok, 1}
%% today: tagged(term()) -> term()
tagged(X) ->
    case X of
        1 -> {ok, 1};
        _ -> {error, bad}
    end.

%% Maps.

%% expect: atom_map() -> #{a := 1, b := 2}
%% today: atom_map() -> term()
atom_map() -> #{a => 1, b => 2}.

%% expect: integer_map() -> #{1 := one, 2 := two}
%% today: integer_map() -> term()
integer_map() -> #{1 => one, 2 => two}.

%% expect: tuple_key_map() -> #{{a, 1} := x}
%% today: tuple_key_map() -> term()
tuple_key_map() -> #{{a, 1} => x}.

%% expect: mixed_map() -> #{2 := b, a := 1, {k} := float()}
%% today: mixed_map() -> term()
mixed_map() -> #{a => 1, 2 => b, {k} => 3.0}.

%% expect: updated_map(term()) -> map()
%% today: updated_map(term()) -> term()
updated_map(M) -> M#{a => 1}.

%% Functions returning functions, and local funs.

%% expect: returns_fun() -> fun(() -> 42)
%% today: returns_fun() -> term()
returns_fun() -> fun integer/0.

%% expect: returns_remote_fun() -> fun((term()) -> term())
%% today: returns_remote_fun() -> term()
returns_remote_fun() -> fun lists:reverse/1.

%% expect: returns_closure(term()) -> fun((term()) -> number())
%% today: returns_closure(term()) -> term()
returns_closure(X) -> fun(Y) -> X + Y end.

%% expect: applies_fun() -> 6
%% today: applies_fun() -> term()
applies_fun() ->
    Double = fun(Y) -> Y * 2 end,
    Double(3).

%% expect: local_fun() -> fun(() -> 5)
%% today: local_fun() -> term()
local_fun() ->
    Five = fun() -> 5 end,
    Five.

%% Other values.

%% expect: binary() -> <<_:16>>
%% today: binary() -> term()
binary() -> <<1, 2>>.

%% expect: identity(term()) -> argument 1
identity(X) -> X.

%% expect: second(term(), term()) -> argument 2
second(_, Y) -> Y.
