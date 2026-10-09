%% Dependent facts (tests/compiler/inference/expectations.py, docs/semantic.md#dependent-facts): the value of a case
%% or if depends on the variables its clauses narrow, one function type per clause, and a function whose result
%% depends on its arguments splits into one function type per clause of that value.
-module(dependent).
-export([
    bound_read/1,
    narrowed_read/1,
    typed_read/1,
    alias/1,
    local/1,
    both/2,
    many/5,
    tens/1,
    raising/1,
    nested_if/2,
    used/1,
    shared/1,
    plus_one/1,
    back/1,
    pair/2,
    twice/1,
    called/1,
    wide/3,
    tuple_case/2,
    tuple_bound/2,
    try_plain/1,
    try_caught/1,
    try_tuple/2,
    received/1
]).

%% A variable bound to a case value keeps its dependence.
%% expect: bound_read(1) -> one; (_) -> other
bound_read(X) ->
    Y =
        case X of
            1 -> one;
            _ -> other
        end,
    Y.

%% A read selects the clauses the parameter's narrowed fact enters.
%% expect: narrowed_read(1) -> one; (_) -> none
narrowed_read(X) ->
    Y =
        case X of
            1 -> one;
            2 -> two;
            _ -> other
        end,
    case X of
        1 -> Y;
        _ -> none
    end.

%% A type test leaves out the clauses of other types.
%% expect: typed_read(integer()) -> integer(); (_) -> zero
typed_read(X) ->
    Y =
        case X of
            N when is_integer(N) -> N + 1;
            _ -> 0
        end,
    if
        is_integer(X) -> Y;
        true -> zero
    end.

%% A parameter bound to an argument's value names that argument.
%% expect: alias(1) -> one; (_) -> other
alias(X) ->
    Y = X,
    case Y of
        1 -> one;
        _ -> other
    end.

%% A parameter that is no argument adds no function type.
%% expect: local(tuple()) -> 1..2
local(X) ->
    Z = element(1, X),
    case Z of
        a -> 1;
        _ -> 2
    end.

%% An if depends on every variable its guards test.
%% expect: both(integer(), atom()) -> both; (integer(), _) -> int; (_, _) -> other
both(X, Y) ->
    if
        is_integer(X), is_atom(Y) -> both;
        is_integer(X) -> int;
        true -> other
    end.

%% At most 4 parameters: the fifth variable's test is forgotten.
%% expect: many(atom(), atom(), atom(), atom(), _) -> atoms; (_, _, _, _, _) -> other
many(A, B, C, D, E) ->
    if
        is_atom(A), is_atom(B), is_atom(C), is_atom(D), is_atom(E) -> atoms;
        true -> other
    end.

%% At most 8 function types: the last ones merge.
%% expect: tens(1) -> a; (2) -> b; (3) -> c; (4) -> d; (5) -> e; (6) -> f; (7) -> g; (8..10) -> h | i | j
tens(X) ->
    case X of
        1 -> a;
        2 -> b;
        3 -> c;
        4 -> d;
        5 -> e;
        6 -> f;
        7 -> g;
        8 -> h;
        9 -> i;
        10 -> j
    end.

%% A clause that always raises keeps its type with none().
%% expect: raising(0) -> none(); (X) -> X
raising(X) ->
    case X of
        0 -> error(zero);
        _ -> X
    end.

%% An if inside a case clause flattens into the case's parameters.
%% expect: nested_if(ok, integer()) -> int; (ok, _) -> other; (_, _) -> error
nested_if(X, Y) ->
    case X of
        ok ->
            if
                is_integer(Y) -> int;
                true -> other
            end;
        _ ->
            error
    end.

%% A use that narrows a dependent variable keeps the clauses whose values it can have.
%% expect: used(a) -> 1
used(X) ->
    Y =
        case X of
            a -> 1;
            _ -> two
        end,
    _ = Y + 1,
    Y.

%% A variable every clause binds depends on the parameters like the case value.
%% expect: shared(1) -> 5; (_) -> 6
shared(X) ->
    case X of
        1 -> Y = 5;
        _ -> Y = 6
    end,
    Y.

%% An operation on a dependent value is evaluated once per clause.
%% expect: plus_one(a) -> 2; (b) -> 3
plus_one(X) ->
    R =
        case X of
            a -> 1;
            b -> 2
        end,
    R + 1.

%% Narrowing a dependent value narrows its parameters.
%% expect: back(a | b) -> a | other
back(X) ->
    R =
        case X of
            a -> 1;
            b -> 2
        end,
    case R of
        1 -> X;
        _ -> other
    end.

%% Two dependent operands combine clause by clause.
%% expect: pair(1, 1) -> {one, one}; (1, _) -> {one, many}; (_, 1) -> {many, one}; (_, _) -> {many, many}
pair(X, Y) ->
    A =
        case X of
            1 -> one;
            _ -> many
        end,
    B =
        case Y of
            1 -> one;
            _ -> many
        end,
    {A, B}.

%% Reads of one variable take the same clause.
%% expect: twice(1) -> 2; (_) -> 4
twice(X) ->
    Y =
        case X of
            1 -> 1;
            _ -> 2
        end,
    Y + Y.

%% A call of a batch function selects its function types once per clause.
%% expect: called(1) -> 1; (_) -> 2
called(X) ->
    Y =
        case X of
            1 -> one;
            _ -> other
        end,
    tag(Y).

%% expect: tag(one) -> 1; (other) -> 2
tag(one) -> 1;
tag(other) -> 2.

%% Past 16 combinations a use is the plain join.
%% expect: wide(_, _, _) -> {a | b | c, a | b | c, a | b | c}
wide(A, B, C) ->
    X =
        case A of
            1 -> a;
            2 -> b;
            _ -> c
        end,
    Y =
        case B of
            1 -> a;
            2 -> b;
            _ -> c
        end,
    Z =
        case C of
            1 -> a;
            2 -> b;
            _ -> c
        end,
    {X, Y, Z}.

%% A tuple of variables narrows each variable by its element.
%% expect: tuple_case(1, a) -> one; (_, _) -> other
tuple_case(X, Y) ->
    case {X, Y} of
        {1, a} -> one;
        _ -> other
    end.

%% A tuple pattern's variable names the scrutinee's element: its guard narrows that variable too.
%% expect: tuple_bound(integer(), b) -> integer(); (_, _) -> none
tuple_bound(X, Y) ->
    case {X, Y} of
        {A, b} when is_integer(A) -> A + 1;
        _ -> none
    end.

%% A try ... of depends on its body's value like a case.
%% expect: try_plain(1) -> one; (_) -> other
try_plain(X) ->
    try X of
        1 -> one;
        _ -> other
    after
        ok
    end.

%% Catch clauses can follow any value: their values join into every clause.
%% expect: try_caught(1) -> error | one; (_) -> error | other
try_caught(X) ->
    try X of
        1 -> one;
        _ -> other
    catch
        _:_ -> error
    end.

%% A try ... of on a tuple of variables.
%% expect: try_tuple(1, _) -> one; (_, _) -> other
try_tuple(X, Y) ->
    try {X, Y} of
        {1, _} -> one;
        _ -> other
    after
        ok
    end.

%% A receive matches a message nothing outside can name: its value stays the join.
%% expect: received(_) -> one | other
received(X) ->
    receive
        X -> one;
        _ -> other
    end.
