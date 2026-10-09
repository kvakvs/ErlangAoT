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
    returns_fun/0,
    returns_remote_fun/0,
    returns_closure/1,
    applies_fun/0,
    local_fun/0,
    binary/0,
    unicode_binary/0,
    identity/1,
    second/2
]).

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
%% today: same_list() -> term()
same_list() -> [1, 2, 3].

%% expect: mixed_list() -> [1 | float() | a, ...]
%% today: mixed_list() -> term()
mixed_list() -> [1, a, 2.5].

%% expect: nested_list() -> [[1 | 2 | 3, ...], ...]
%% today: nested_list() -> term()
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
binary() -> <<1, 2>>.

%% UTF-8 sizes are exact for literal characters: 1 + 2 bytes.
%% expect: unicode_binary() -> <<_:24>>
unicode_binary() -> <<"a\x{e9}"/utf8>>.

%% expect: identity(term()) -> argument 1
identity(X) -> X.

%% expect: second(term(), term()) -> argument 2
second(_, Y) -> Y.
