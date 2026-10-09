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
    two_callers/0,
    countdown_start/0,
    fun_target/0,
    pick/1,
    pick_number/1,
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
    consecutive/1,
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
    with_tail/1,
    list_same_shape/1,
    list_other_shape/1,
    map_join/1,
    record_match/1,
    not_record/0,
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
    inc/1,
    len/1,
    after_inc/1,
    caught/1,
    branch/2,
    first/1,
    second_element/1,
    lookup/1,
    remote/1,
    aliased/1,
    bits/1,
    both/1,
    orelse_use/2,
    try_of/1,
    try_body/1,
    try_classes/0,
    try_impossible/1,
    try_after/1,
    try_nested/1,
    try_of_use/1,
    try_catch_use/1,
    maybe_else/1,
    maybe_failed/1,
    maybe_chain/1,
    maybe_impossible/0,
    maybe_plain/0,
    maybe_after/1,
    second/2,
    cons_unknown/2,
    cons_list/1
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

%% expect: remainder_range(integer()) -> -9..9
remainder_range(X) -> X rem 10.

%% expect: masked(integer()) -> 0..15
masked(X) -> X band 15.

%% expect: mixed_sum() -> float()
mixed_sum() -> 1 + 2.5.

%% expect: bad_sum() -> none()
bad_sum() -> 1 + a.

%% expect: divide_by_zero() -> none()
divide_by_zero() -> 1 div 0.

%% expect: negated(number()) -> number()
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

%% expect: unknown_order(_) -> boolean()
unknown_order(X) -> X =< 1.

%% Builtins.

%% expect: absolute() -> 5
absolute() -> abs(-5).

%% expect: rounded() -> integer()
rounded() -> round(2.5).

%% expect: minimum() -> 1..2
minimum() -> min(1, 2).

%% expect: displayed() -> true
displayed() -> erlang:display(x).

%% expect: formatted() -> ok
formatted() -> io:format("x~n").

%% expect: sent(_) -> hello
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
call_local() -> increment(3).

%% expect: increment(3) -> 4
increment(X) -> X + 1.

%% A local function's inputs join its call sites' arguments; recursive calls count too, widening as results do. Each
%% call evaluates the callee again with its own arguments (58M).
%% expect: two_callers() -> {11, 21}
two_callers() -> {add_one(10), add_one(20)}.

%% expect: add_one(10 | 20) -> 11..21
add_one(X) -> X + 1.

%% expect: countdown_start() -> done
countdown_start() -> countdown(3).

%% expect: countdown(0) -> done; (integer()) -> done
countdown(0) -> done;
countdown(N) -> countdown(N - 1).

%% A local function that fun F/A names may be called from anywhere.
%% expect: fun_target() -> fun((_) -> number())
fun_target() -> fun doubled/1.

%% expect: doubled(number()) -> number()
doubled(X) -> X * 2.

%% A local function nothing calls never runs.
%% expect: uncalled(none()) -> none()
uncalled(X) -> X.

%% Integers, integer ranges and integers or floats.

%% expect: pick(a) -> 1; (b) -> 2; (_) -> 3
pick(X) ->
    case X of
        a -> 1;
        b -> 2;
        _ -> 3
    end.

%% expect: clauses(_) -> 10 | 20
clauses(X) when X > 0 -> 10;
clauses(_) -> 20.

%% expect: bounded(1..10) -> 1..10
bounded(X) when is_integer(X), X >= 1, X =< 10 -> X.

%% expect: integer_or_float(1) -> 1; (_) -> float()
integer_or_float(X) ->
    case X of
        1 -> 1;
        _ -> 2.5
    end.

%% expect: scaled(integer()) -> integer(); (float()) -> float()
scaled(X) when is_integer(X) -> X * 2;
scaled(X) when is_float(X) -> X / 2.

%% Lists.

%% expect: same_list() -> [1, 2, 3]
same_list() -> [1, 2, 3].

%% expect: mixed_list() -> [1, a, float()]
mixed_list() -> [1, a, 2.5].

%% expect: nested_list() -> [[1, ...], [2, 3]]
nested_list() -> [[1], [2, 3]].

%% expect: empty_list() -> []
empty_list() -> [].

%% expect: string() -> [97..99, ...]
string() -> "abc".

%% A string of eight different characters keeps them; a ninth makes their range, gaps included. Consecutive
%% integers print as a range.
%% expect: eight_letters() -> [97 | 99 | 101 | 103 | 105 | 107 | 109 | 111, ...]
eight_letters() -> "acegikmo".

%% expect: nine_letters() -> [97..113, ...]
nine_letters() -> "acegikmoq".

%% expect: consecutive(_) -> 1..3 | 5 | 7..8
consecutive(X) -> pick_number(X).

%% expect: pick_number(1) -> 1; (2) -> 2; (3) -> 3; (5) -> 5; (7) -> 7; (_) -> 8
pick_number(X) ->
    case X of
        1 -> 1;
        2 -> 2;
        3 -> 3;
        5 -> 5;
        7 -> 7;
        _ -> 8
    end.

%% Tuples.

%% expect: same_tuple() -> {1, 2, 3}
same_tuple() -> {1, 2, 3}.

%% expect: mixed_tuple() -> {ok, 1, float()}
mixed_tuple() -> {ok, 1, 2.5}.

%% expect: nested_tuple() -> {{a, 1}, {b, 2}}
nested_tuple() -> {{a, 1}, {b, 2}}.

%% expect: tagged(1) -> {ok, 1}; (_) -> {error, bad}
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

%% expect: updated_map(map()) -> map()
updated_map(M) -> M#{a => 1}.

%% Records are tuples; containers taken apart give back their elements' facts.

%% expect: record() -> #point{x :: 0, y :: undefined}
record() -> #point{}.

%% expect: record_field() -> 3
record_field() -> (#point{x = 3})#point.x.

%% expect: record_update() -> #point{x :: 0, y :: 5}
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

%% expect: head_value() -> 1
head_value() -> hd([1, 2]).

%% expect: tail_value() -> [2, ...]
tail_value() -> tl([1, 2]).

%% expect: cons_cell() -> [a | b, ...]
cons_cell() -> [a | [b]].

%% expect: appended() -> [1, 2, 3]
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

%% expect: list_of_tuple() -> [1, a]
list_of_tuple() -> tuple_to_list({1, a}).

%% A tuple of more than 16 known elements keeps them; a map of more than 16 keys joins them into one association;
%% past 4 levels an inner value is _.
%% expect: wide_tuple() -> {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}
wide_tuple() -> {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}.

%% expect: deep_tuple() -> {{{{_}}}}
deep_tuple() -> {{{{{1}}}}}.

%% expect: wide_map() -> #{1..17 => a}
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

%% Lists of two or more known elements keep their positions (a Clause notation: the type language has none), unions
%% among them in parentheses; shapes of other lengths join as plain lists. Maps of other keys join into one
%% association; a tuple of a record's name and size prints as the record.

%% expect: with_tail(_) -> [a, b | _]
with_tail(T) -> [a, b | T].

%% expect: list_same_shape(1) -> [1, a]; (_) -> [2, b]
list_same_shape(X) ->
    case X of
        1 -> [1, a];
        _ -> [2, b]
    end.

%% expect: list_other_shape(1) -> [1, 2]; (_) -> [a, ...]
list_other_shape(X) ->
    case X of
        1 -> [1, 2];
        _ -> [a]
    end.

%% expect: map_join(1) -> #{a := 1}; (_) -> #{b := 2}
map_join(X) ->
    case X of
        1 -> #{a => 1};
        _ -> #{b => 2}
    end.

%% expect: record_match(#point{x :: _, y :: _}) -> _
record_match(#point{x = X}) -> X.

%% expect: not_record() -> {other, 1, 2}
not_record() -> {other, 1, 2}.

%% Functions returning functions, and local funs.

%% expect: returns_fun() -> fun(() -> 42)
returns_fun() -> fun integer/0.

%% expect: returns_remote_fun() -> fun((_) -> _)
returns_remote_fun() -> fun lists:reverse/1.

%% expect: returns_closure(_) -> fun((_) -> number())
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
%% expect: named_fun() -> fun((0) -> 1; (_) -> number())
named_fun() ->
    fun
        Fact(0) -> 1;
        Fact(N) -> N * Fact(N - 1)
    end.

%% expect: mapped() -> [_, ...]
mapped() -> lists:map(fun(X) -> X * 2 end, [1, 2]).

%% expect: stored_fun() -> {fun(() -> 1)}
stored_fun() -> {fun() -> 1 end}.

%% expect: called_from_tuple() -> 1
called_from_tuple() -> (element(1, {fun() -> 1 end}))().

%% expect: unknown_call(fun((_) -> _)) -> _
unknown_call(F) -> F(1).

%% expect: applied() -> _
applied() -> apply(fun() -> 1 end, []).

%% A call with the wrong number of arguments raises badarity.
%% expect: wrong_arity() -> none()
wrong_arity() ->
    F = fun() -> 1 end,
    F(2).

%% expect: builtin_fun() -> fun((_) -> _)
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

%% An operation that raises unless its operand has a type proves that type after it returns; a function's inputs are
%% its success domain, what its arguments are when it returns.

%% expect: inc(number()) -> number()
inc(X) -> X + 1.

%% expect: len(list()) -> non_neg_integer()
len(L) -> length(L).

%% expect: after_inc(number()) -> number()
after_inc(X) ->
    _ = inc(X),
    X.

%% A use inside try, or in one case branch, proves nothing after it.
%% expect: caught(X) -> X
caught(X) ->
    _ =
        try
            X + 1
        catch
            _:_ -> 0
        end,
    X.

%% expect: branch(_, X) -> X
branch(A, X) ->
    _ =
        case A of
            1 -> X + 1;
            _ -> 0
        end,
    X.

%% expect: first(nonempty_maybe_improper_list()) -> _
first(L) -> hd(L).

%% expect: second_element(tuple()) -> _
second_element(T) -> element(2, T).

%% expect: lookup(map()) -> _
lookup(M) -> map_get(a, M).

%% expect: remote(atom()) -> _
remote(M) -> M:start().

%% Every name bound to the same value narrows with it.
%% expect: aliased(number()) -> number()
aliased(X) ->
    Y = X,
    _ = Y + 1,
    X.

%% expect: bits(integer()) -> <<_:8>>
bits(X) -> <<X:8>>.

%% expect: both(boolean()) -> boolean()
both(X) -> X andalso true.

%% The right operand of orelse may not run.
%% expect: orelse_use(boolean(), Y) -> Y
orelse_use(X, Y) ->
    _ = X orelse Y + 1 > 0,
    Y.

%% expect: identity(X) -> X
identity(X) -> X.

%% expect: second(_, Y) -> Y
second(_, Y) -> Y.

%% try and maybe values (step 58J1).

%% A try joins its of clauses' and catch clauses' values.
%% expect: try_of(_) -> error | two | {number()}
try_of(X) ->
    try X + 1 of
        2 -> two;
        N -> {N}
    catch
        _:_ -> error
    end.

%% Without of, the body's value.
%% expect: try_body(_) -> number()
try_body(X) ->
    try
        X * 2
    catch
        _:_ -> 0
    end.

%% A class variable is one of error, exit and throw.
%% expect: try_classes() -> error | exit | throw
try_classes() ->
    try
        error(failed)
    catch
        Class:_ -> Class
    end.

%% An of clause the body's value cannot match adds nothing.
%% expect: try_impossible(integer()) -> integer() | none
try_impossible(X) when is_integer(X) ->
    try X of
        a -> atom;
        N -> N
    catch
        _ -> none
    end.

%% The after body adds nothing.
%% expect: try_after(atom()) -> atom()
try_after(X) when is_atom(X) ->
    try
        X
    after
        ok
    end.

%% expect: try_nested(_) -> inner | one | outer
try_nested(X) ->
    try
        try X of
            1 -> one
        catch
            _:_ -> inner
        end
    catch
        _:_ -> outer
    end.

%% Of clauses run after the body returned: its uses hold there.
%% expect: try_of_use(_) -> number()
try_of_use(X) ->
    try X + 1 of
        _ -> X
    catch
        _:_ -> 0
    end.

%% A catch clause sees the facts from before the try.
%% expect: try_catch_use(_) -> _
try_catch_use(X) ->
    try
        X + 1
    catch
        _:_ -> X
    end.

%% A maybe joins its body's value and its else clauses' values.
%% expect: maybe_else(_) -> number() | bad
maybe_else(X) ->
    maybe
        {ok, V} ?= X,
        V + 1
    else
        _ -> bad
    end.

%% Without else, the values a ?= match fails on are the maybe's value.
%% expect: maybe_failed(_) -> 1 | error
maybe_failed(X) ->
    V =
        case X of
            1 -> {ok, 1};
            _ -> error
        end,
    maybe
        {ok, Y} ?= V,
        Y
    end.

%% Else clauses match the values every ?= match of the chain fails on.
%% expect: maybe_chain(_) -> failed | {_} | {other, _}
maybe_chain(X) ->
    maybe
        {ok, A} ?= X,
        {ok, B} ?= A,
        {B}
    else
        error -> failed;
        Other -> {other, Other}
    end.

%% A ?= match that never succeeds stops the body.
%% expect: maybe_impossible() -> {failed, error}
maybe_impossible() ->
    maybe
        {ok, V} ?= error,
        V
    else
        E -> {failed, E}
    end.

%% expect: maybe_plain() -> 2
maybe_plain() ->
    maybe
        1,
        2
    end.

%% A ?= match narrows only within the maybe body.
%% expect: maybe_after(X) -> X
maybe_after(X) ->
    _ =
        maybe
            ok ?= X
        end,
    X.

%% A cell in front of an unknown tail is never empty, though it may end improperly.
%% expect: cons_unknown(_, _) -> nonempty_maybe_improper_list()
cons_unknown(H, T) -> [H | T].

%% In front of a possibly empty list: a nonempty list.
%% expect: cons_list(maybe_improper_list()) -> nonempty_maybe_improper_list()
cons_list(T) when is_list(T) -> [1 | T].
