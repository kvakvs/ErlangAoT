-module(answer).
-export([
    str_alias_1/1,
    tuple_alias_a/1,
    tuple_alias_b/1,
    list_in_tuple_a/1,
    list_in_tuple_b/1,
    tuple_in_tuple_a/1,
    tuple_in_tuple_b/1,
    multiple_aliases_1a/1,
    multiple_aliases_1b/1,
    multiple_aliases_2/1,
    multiple_aliases_3a/1,
    multiple_aliases_3b/1,
    multiple_aliases_4a/1,
    multiple_aliases_4b/1,
    list_alias1a/1,
    list_alias1b/1,
    list_alias2a/1,
    list_alias2b/1,
    list_alias3a/1,
    list_alias3b/1,
    first/2,
    id/1,
    do_tuple/0,
    do_literal_tuple_1/1,
    do_literal_tuple_2/1,
    make/1,
    head_tail/1,
    prefix/1,
    nested_prefix/1,
    empty_prefix/1,
    repeat/2,
    independent/1,
    extract/1,
    body/1,
    fallthrough/1,
    badmatch/1,
    order/2,
    queries/1,
    guard/1,
    guard_construct/1,
    head/1,
    tail/1,
    length_value/1,
    size_value/1,
    element_value/2,
    wrong_spec/1,
    source_order/1,
    failure_order/1,
    wide/1
]).
str_alias_1("a" ++ "bc" = Letters) ->
    [97, 98, 99] = Letters,
    ok;
str_alias_1("def" = [$d | Rest]) ->
    "ef" = Rest,
    ok;
str_alias_1("gh" ++ "i" = Letters) ->
    "ghi" = Letters,
    ok;
str_alias_1("klm" = [$k | Rest]) ->
    "lm" = Rest,
    ok;
str_alias_1("qr" ++ "s" = Letters) ->
    "qrs" = Letters,
    ok;
str_alias_1("xy" = [$x, $y]) ->
    ok;
str_alias_1([] = Empty) ->
    "" = Empty,
    ok;
str_alias_1(_) ->
    error.
tuple_alias_a(Whole = {Left, Middle, Right}) ->
    {AgainLeft, AgainMiddle, AgainRight} = Whole,
    {Left, Middle, Right, AgainLeft, AgainMiddle, AgainRight};
tuple_alias_a(Whole = {Left, Right}) ->
    {SecondLeft, SecondRight} = Third = Whole,
    {ThirdLeft, ThirdRight} = Third,
    {Left, Right, SecondLeft, SecondRight, ThirdLeft, ThirdRight}.
tuple_alias_b(Input = {_, _, _}) ->
    First = {Left, Middle, Right} = Input,
    {CopyLeft, CopyMiddle, CopyRight} = First,
    {Left, Middle, Right, CopyLeft, CopyMiddle, CopyRight};
tuple_alias_b(Input = {_, _}) ->
    First = {Left, Right} = Input,
    {CopyLeft, CopyRight} = Second = First,
    {LastLeft, LastRight} = Second,
    {Left, Right, CopyLeft, CopyRight, LastLeft, LastRight}.
list_in_tuple_a({container, Items = [First, Second, Third], Extra}) ->
    [_, _, _] = Items,
    {First, Second, Third, Extra}.
list_in_tuple_b(Input) ->
    {container, Items, Extra} = Input,
    ([First, Second, Third] = [_, _, _]) = Items,
    {First, Second, Third, Extra}.
tuple_in_tuple_a(Outer = {x, Inner = {y, Item}, Last}) ->
    {InnerTag, InnerValue} = Inner,
    {OuterTag, OuterValue, OuterLast} = Outer,
    {Item, InnerTag, InnerValue, Last, OuterTag, OuterValue, OuterLast}.
tuple_in_tuple_b(Input) ->
    {OuterTag, Inner, Last} = Input,
    {x, {y, Item}, Last} = Input,
    {InnerTag, InnerValue} = Inner,
    {Item, InnerTag, InnerValue, Last, OuterTag, Inner, Last}.
multiple_aliases_1a((Left = Right) = (AgainLeft = AgainRight)) ->
    {AgainLeft, AgainRight, Left, Right}.
multiple_aliases_1b(Input) ->
    Left = Right = Input,
    AgainLeft = AgainRight = Right,
    {Left, AgainLeft, Right, AgainRight}.
multiple_aliases_2((First = Second) = (Third = First)) -> {Third, First, Second}.
multiple_aliases_3a(First = {_, _} = (Second = Third)) -> {Third, First, Second}.
multiple_aliases_3b(Input) ->
    First = {_, _} = Input,
    Second = Third = First,
    {Third, First, Second}.
multiple_aliases_4a(First = [_, _, _] = (Second = Third)) -> {Third, First, Second}.
multiple_aliases_4b(Input) ->
    First = [_, _, _] = Input,
    Second = Third = First,
    {Third, First, Second}.
list_alias1a(Items = [a, b]) ->
    [First, Second] = Items,
    {First, Second}.
list_alias1b(Input) ->
    Items = [a, b] = Input,
    [First, Second] = Items,
    {First, Second}.
list_alias2a([First, Second] = Items) ->
    [a, b] = Items,
    {First, Second}.
list_alias2b(Input) ->
    [First, Second] = Items = Input,
    [a, b] = Items,
    {First, Second}.
list_alias3a([First, b] = [a, Second]) -> {First, Second}.
list_alias3b(Input) ->
    [First, b] = Input,
    [a, Second] = Input,
    {First, Second}.
first(Selected, Discarded) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
id(Value) ->
    Saved = Value,
    Saved.
do_tuple() ->
    Value = {local_tuple},
    {0, _} = Value.
do_literal_tuple_1(Index) ->
    Values = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    element(Index, Values).
do_literal_tuple_2(Index) ->
    Values = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    element(Index, Values).
make(X) -> {id(X), [X, {X}, "Î©" | X], "abc"}.
head_tail(_) ->
    Items = id([1]),
    [Only] = Items,
    Only = hd(Items),
    [] = tl(Items),
    ok.
prefix("ab" ++ T) -> T;
prefix(_) -> no.
nested_prefix([97 | [98 | []]] ++ T) -> T;
nested_prefix(_) -> no.
empty_prefix([] ++ T) -> T.
repeat(X, X) -> equal;
repeat(_, _) -> different.
independent(X) ->
    A = {X, [X]},
    B = {X, [X]},
    A = B,
    A =:= B.
extract({_, [A | T]}) -> id({A, T});
extract(X) -> X.
body(X) ->
    {A, [B | T]} = X,
    {A, B, T}.
fallthrough({A, A}) -> A;
fallthrough([A, A]) -> A;
fallthrough({_, A}) -> A;
fallthrough(X) -> X.
badmatch(X) ->
    {impossible} = {X, [X]},
    unreachable.
order(X, Y) -> {X =:= Y, X == Y, X < Y, X =< Y, X > Y, X >= Y, min(X, Y), max(X, Y)}.
queries(X) -> {is_tuple(X), is_list(X), is_atom(X), is_integer(X)}.
guard(X) when length(X) =:= 2 -> two;
guard(X) when tuple_size(X) =:= 2 -> pair;
guard(_) -> other.
guard_construct(X) when element(2, {a, X}) =:= hd([X]) -> X.
head(X) -> hd(X).
tail(X) -> tl(X).
length_value(X) -> length(X).
size_value(X) -> size(X).
element_value(N, X) -> element(N, X).
-spec wrong_spec(integer()) -> integer().
wrong_spec({_, X}) -> X;
wrong_spec([X | _]) -> X;
wrong_spec(X) -> X.
source_order(X) ->
    T = {A = id(X), B = id(X)},
    {T, A, B}.
failure_order(X) -> {hd(X), ok = impossible}.
wide(X) ->
    {X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
        X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
        X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
        X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
        X, X, X}.
