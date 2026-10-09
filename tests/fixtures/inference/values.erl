%% Type inference expectations (tests/compiler/inference/expectations.py, docs/semantic.md#inference-expectations).
%% `expect:` above a function is the signature --print-types should infer for it; `today:` is what inference finds
%% now when it falls short. Exported functions' inputs are always term(); local functions see their callers.
-module(values).
-export([
    integer/0,
    negative/0,
    float/0,
    atom/0,
    character/0,
    negative_float/0,
    big_integer/0,
    quoted_atom/0,
    sum/0,
    product/0,
    division/0,
    comparison/0,
    conjunction/0,
    big_product/0,
    shifted/0,
    quotient/0,
    remainder/0,
    remainder_range/1,
    masked/1,
    mixed_sum/0,
    bad_sum/0,
    divide_by_zero/0,
    negated/1,
    complemented/0,
    exclusive/0,
    negation/0,
    atom_order/0,
    number_below_atom/0,
    unknown_order/1,
    absolute/0,
    rounded/0,
    minimum/0,
    displayed/0,
    formatted/0,
    sent/1,
    raised/0,
    node_name/0,
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
    eight_letters/0,
    nine_letters/0,
    same_tuple/0,
    mixed_tuple/0,
    nested_tuple/0,
    tagged/1,
    atom_map/0,
    integer_map/0,
    tuple_key_map/0,
    mixed_map/0,
    updated_map/1,
    record/0,
    record_field/0,
    record_update/0,
    record_index/0,
    first_element/0,
    matched_element/0,
    case_element/0,
    head_value/0,
    tail_value/0,
    cons_cell/0,
    appended/0,
    map_lookup/0,
    map_known_update/0,
    map_exact_update/0,
    comprehension/0,
    list_of_tuple/0,
    wide_tuple/0,
    deep_tuple/0,
    wide_map/0,
    returns_fun/0,
    returns_remote_fun/0,
    returns_closure/1,
    applies_fun/0,
    local_fun/0,
    named_fun/0,
    mapped/0,
    stored_fun/0,
    called_from_tuple/0,
    unknown_call/1,
    applied/0,
    wrong_arity/0,
    builtin_fun/0,
    forward_fun/0,
    binary/0,
    unicode_binary/0,
    identity/1,
    second/2
]).

-record(point, {x = 0, y}).

%% Simple expressions.

%% expect: integer() -> 42
integer() -> 42.

%% expect: negative() -> -7
negative() -> -7.

%% expect: float() -> float()
float() -> 2.5.

%% expect: atom() -> ok
atom() -> ok.

%% expect: character() -> 120
character() -> $x.

%% expect: negative_float() -> float()
negative_float() -> -2.5.

%% expect: big_integer() -> 123456789012345678901234567890
big_integer() -> 123456789012345678901234567890.

%% expect: quoted_atom() -> 'hello world'
quoted_atom() -> 'hello world'.

%% expect: sum() -> 3
sum() -> 1 + 2.

%% expect: product() -> 42
product() -> 6 * 7.

%% expect: division() -> float()
division() -> 7 / 2.

%% expect: comparison() -> true
comparison() -> 1 < 2.

%% expect: conjunction() -> false
conjunction() -> true andalso false.

%% Integer arithmetic folds exactly, also past a word, and keeps runtime failures: a result that always raises is
%% none().
%% expect: big_product() -> 1219326311370217952237463801111263526900
big_product() -> 12345678901234567890 * 98765432109876543210.

%% expect: shifted() -> 1267650600228229401496703205376
shifted() -> 1 bsl 100.

%% expect: quotient() -> 3
quotient() -> 17 div 5.

%% expect: remainder() -> -2
remainder() -> -17 rem 5.

%% expect: remainder_range(term()) -> -9..9
remainder_range(X) -> X rem 10.

%% expect: masked(term()) -> 0..15
masked(X) -> X band 15.

%% expect: mixed_sum() -> float()
mixed_sum() -> 1 + 2.5.

%% expect: bad_sum() -> none()
bad_sum() -> 1 + a.

%% expect: divide_by_zero() -> none()
divide_by_zero() -> 1 div 0.

%% expect: negated(term()) -> number()
negated(X) -> -X.

%% expect: complemented() -> -6
complemented() -> bnot 5.

%% Booleans and comparisons.

%% expect: exclusive() -> true
exclusive() -> true xor false.

%% expect: negation() -> false
negation() -> not true.

%% expect: atom_order() -> true
atom_order() -> a < b.

%% expect: number_below_atom() -> true
number_below_atom() -> 1 < a.

%% expect: unknown_order(term()) -> boolean()
unknown_order(X) -> X =< 1.

%% Builtins.

%% expect: absolute() -> 5
absolute() -> abs(-5).

%% expect: rounded() -> integer()
rounded() -> round(2.5).

%% expect: minimum() -> 1 | 2
minimum() -> min(1, 2).

%% expect: displayed() -> true
displayed() -> erlang:display(x).

%% expect: formatted() -> ok
formatted() -> io:format("x~n").

%% expect: sent(term()) -> hello
sent(Pid) -> Pid ! hello.

%% erlang:raise/3 returns only for an invalid class.
%% expect: raised() -> badarg
raised() -> erlang:raise(bad, reason, []).

%% expect: node_name() -> atom()
node_name() -> node().

%% Functions returning what other functions return.

%% expect: call_integer() -> 42
call_integer() -> integer().

%% expect: call_local() -> 4
%% today: call_local() -> number()
call_local() -> increment(3).

%% expect: increment(3) -> 4
%% today: increment(term()) -> number()
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
integer_or_float(X) ->
    case X of
        1 -> 1;
        _ -> 2.5
    end.

%% expect: scaled(term()) -> number()
scaled(X) when is_integer(X) -> X * 2;
scaled(X) when is_float(X) -> X / 2.

%% Lists.

%% expect: same_list() -> [1 | 2 | 3, ...]
same_list() -> [1, 2, 3].

%% expect: mixed_list() -> [1 | float() | a, ...]
mixed_list() -> [1, a, 2.5].

%% expect: nested_list() -> [[1 | 2 | 3, ...], ...]
nested_list() -> [[1], [2, 3]].

%% expect: empty_list() -> []
empty_list() -> [].

%% expect: string() -> [97 | 98 | 99, ...]
string() -> "abc".

%% A string of eight different characters keeps them; a ninth makes their range.
%% expect: eight_letters() -> [97 | 98 | 99 | 100 | 101 | 102 | 103 | 104, ...]
eight_letters() -> "abcdefgh".

%% expect: nine_letters() -> [97..105, ...]
nine_letters() -> "abcdefghi".

%% Tuples.

%% expect: same_tuple() -> {1, 2, 3}
same_tuple() -> {1, 2, 3}.

%% expect: mixed_tuple() -> {ok, 1, float()}
mixed_tuple() -> {ok, 1, 2.5}.

%% expect: nested_tuple() -> {{a, 1}, {b, 2}}
nested_tuple() -> {{a, 1}, {b, 2}}.

%% expect: tagged(term()) -> {error, bad} | {ok, 1}
tagged(X) ->
    case X of
        1 -> {ok, 1};
        _ -> {error, bad}
    end.

%% Maps.

%% expect: atom_map() -> #{a := 1, b := 2}
atom_map() -> #{a => 1, b => 2}.

%% expect: integer_map() -> #{1 := one, 2 := two}
integer_map() -> #{1 => one, 2 => two}.

%% expect: tuple_key_map() -> #{{a, 1} := x}
tuple_key_map() -> #{{a, 1} => x}.

%% expect: mixed_map() -> #{2 := b, a := 1, {k} := float()}
mixed_map() -> #{a => 1, 2 => b, {k} => 3.0}.

%% expect: updated_map(term()) -> map()
updated_map(M) -> M#{a => 1}.

%% Records are tuples; containers taken apart give back their elements' facts.

%% expect: record() -> {point, 0, undefined}
record() -> #point{}.

%% expect: record_field() -> 3
record_field() -> (#point{x = 3})#point.x.

%% expect: record_update() -> {point, 0, 5}
record_update() -> (#point{})#point{y = 5}.

%% expect: record_index() -> 3
record_index() -> #point.y.

%% expect: first_element() -> a
first_element() -> element(1, {a, 1}).

%% expect: matched_element() -> 2
matched_element() ->
    {_, B} = {1, 2},
    B.

%% expect: case_element() -> 1
case_element() ->
    case {1, 2} of
        {A, _} -> A
    end.

%% expect: head_value() -> 1 | 2
head_value() -> hd([1, 2]).

%% expect: tail_value() -> [1 | 2]
tail_value() -> tl([1, 2]).

%% expect: cons_cell() -> [a | b, ...]
cons_cell() -> [a | [b]].

%% expect: appended() -> [1 | 2 | 3, ...]
appended() -> [1] ++ [2, 3].

%% expect: map_lookup() -> 1
map_lookup() -> map_get(a, #{a => 1}).

%% expect: map_known_update() -> #{a := 2, b := 1}
map_known_update() -> (#{a => 1})#{a => 2, b => 1}.

%% expect: map_exact_update() -> #{a := 2}
map_exact_update() -> (#{a => 1})#{a := 2}.

%% Generator patterns see the elements of their input.
%% expect: comprehension() -> [2..6]
comprehension() -> [X * 2 || X <- [1, 2, 3]].

%% expect: list_of_tuple() -> [1 | a, ...]
list_of_tuple() -> tuple_to_list({1, a}).

%% Past 16 elements a tuple is tuple() and a map map(); past 4 levels an inner value is term().
%% expect: wide_tuple() -> tuple()
wide_tuple() -> {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}.

%% expect: deep_tuple() -> {{{{term()}}}}
deep_tuple() -> {{{{{1}}}}}.

%% expect: wide_map() -> map()
wide_map() ->
    #{
        1 => a,
        2 => a,
        3 => a,
        4 => a,
        5 => a,
        6 => a,
        7 => a,
        8 => a,
        9 => a,
        10 => a,
        11 => a,
        12 => a,
        13 => a,
        14 => a,
        15 => a,
        16 => a,
        17 => a
    }.

%% Functions returning functions, and local funs.

%% expect: returns_fun() -> fun(() -> 42)
returns_fun() -> fun integer/0.

%% expect: returns_remote_fun() -> fun((term()) -> term())
returns_remote_fun() -> fun lists:reverse/1.

%% expect: returns_closure(term()) -> fun((term()) -> number())
returns_closure(X) -> fun(Y) -> X + Y end.

%% expect: applies_fun() -> 6
applies_fun() ->
    Double = fun(Y) -> Y * 2 end,
    Double(3).

%% expect: local_fun() -> fun(() -> 5)
local_fun() ->
    Five = fun() -> 5 end,
    Five.

%% A named fun's calls of itself are unknown.
%% expect: named_fun() -> fun((term()) -> number())
named_fun() ->
    fun
        Fact(0) -> 1;
        Fact(N) -> N * Fact(N - 1)
    end.

%% expect: mapped() -> list()
mapped() -> lists:map(fun(X) -> X * 2 end, [1, 2]).

%% expect: stored_fun() -> {fun(() -> 1)}
stored_fun() -> {fun() -> 1 end}.

%% expect: called_from_tuple() -> 1
called_from_tuple() -> (element(1, {fun() -> 1 end}))().

%% expect: unknown_call(term()) -> term()
unknown_call(F) -> F(1).

%% expect: applied() -> term()
applied() -> apply(fun() -> 1 end, []).

%% A call with the wrong number of arguments raises badarity.
%% expect: wrong_arity() -> none()
wrong_arity() ->
    F = fun() -> 1 end,
    F(2).

%% expect: builtin_fun() -> fun((term()) -> term())
builtin_fun() -> fun abs/1.

%% A fun of a function inferred later reads its final result.
%% expect: forward_fun() -> fun(() -> 7)
forward_fun() -> fun later/0.

%% expect: later() -> 7
later() -> 7.

%% Other values.

%% expect: binary() -> <<_:16>>
binary() -> <<1, 2>>.

%% UTF-8 sizes are exact for literal characters: 1 + 2 bytes.
%% expect: unicode_binary() -> <<_:24>>
unicode_binary() -> <<"a\x{e9}"/utf8>>.

%% expect: identity(term()) -> argument 1
identity(X) -> X.

%% expect: second(term(), term()) -> argument 2
second(_, Y) -> Y.
