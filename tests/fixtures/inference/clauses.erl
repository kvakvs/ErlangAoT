%% Function types (tests/compiler/inference/expectations.py, docs/semantic.md#inference): a function keeps one
%% function type per possible clause, its arguments' facts after the head and guard and the clause's result, and
%% --print-types shows one signature per function type: `f(integer()) -> integer(); (atom()) -> string()`.
-module(clauses).
-export([
    type_tests/1,
    literals/1,
    merged/1,
    impossible_start/0,
    over_budget/1,
    countdown/1,
    single/1,
    case_split/1,
    if_split/1,
    nested_case/2,
    not_last/1,
    anonymous/0,
    named/0,
    same_funs/1,
    different_funs/1,
    raising/1
]).

%% Type tests per clause.
%% expect: type_tests(integer()) -> integer(); (atom()) -> string()
type_tests(X) when is_integer(X) -> X + 1;
type_tests(X) when is_atom(X) -> atom_to_list(X).

%% Literal patterns.
%% expect: literals(1) -> one; (2) -> two; (_) -> many
literals(1) -> one;
literals(2) -> two;
literals(_) -> many.

%% Clauses of equal inputs merge into one function type.
%% expect: merged(atom()) -> a | b; (_) -> c
merged(X) when is_atom(X) -> a;
merged(Y) when is_atom(Y) -> b;
merged(_) -> c.

%% A clause its callers never enter adds no function type.
%% expect: impossible_start() -> 3
impossible_start() -> integer_only(3).

%% expect: integer_only(3) -> 3
integer_only(X) when is_integer(X) -> X;
integer_only(X) when is_atom(X) -> X.

%% More clauses than the budget of 8: the last ones merge into one.
%% expect: over_budget(1) -> 1; (2) -> 2; (3) -> 3; (4) -> 4; (5) -> 5; (6) -> 6; (7) -> 7; (8..10) -> 8..10
over_budget(1) -> 1;
over_budget(2) -> 2;
over_budget(3) -> 3;
over_budget(4) -> 4;
over_budget(5) -> 5;
over_budget(6) -> 6;
over_budget(7) -> 7;
over_budget(8) -> 8;
over_budget(9) -> 9;
over_budget(10) -> 10.

%% A recursive function's function types iterate with its result.
%% expect: countdown(0) -> done; (pos_integer()) -> done
countdown(0) -> done;
countdown(N) when is_integer(N), N > 0 -> countdown(N - 1).

%% One function type prints as the union summary.
%% expect: single(_) -> {_}
single(X) -> {X}.

%% A case on an argument ending the body splits it, one function type per branch.
%% expect: case_split(1) -> one; (integer()) -> integer(); (_) -> other
case_split(X) ->
    case X of
        1 -> one;
        N when is_integer(N) -> N * 2;
        _ -> other
    end.

%% So does an if.
%% expect: if_split(integer()) -> int; (float()) -> float; (_) -> other
if_split(X) ->
    if
        is_integer(X) -> int;
        is_float(X) -> float;
        true -> other
    end.

%% A nested case does not split further.
%% expect: nested_case(1, _) -> one | uno; (_, _) -> other
nested_case(X, Language) ->
    case X of
        1 ->
            case Language of
                spanish -> uno;
                _ -> one
            end;
        _ ->
            other
    end.

%% A case that does not end the body does not split it.
%% expect: not_last(_) -> {one | other}
not_last(X) ->
    Y =
        case X of
            1 -> one;
            _ -> other
        end,
    {Y}.

%% An anonymous fun keeps a function type per clause.
%% expect: anonymous() -> fun((1) -> one; (atom()) -> atom(); (_) -> other)
anonymous() ->
    fun
        (1) -> one;
        (X) when is_atom(X) -> X;
        (_) -> other
    end.

%% fun F/A has the function types of F/A.
%% expect: named() -> fun((integer()) -> integer(); (atom()) -> string())
named() -> fun type_tests/1.

%% Equal funs join as themselves.
%% expect: same_funs(_) -> fun((integer()) -> integer())
same_funs(X) ->
    F =
        case X of
            1 -> fun(A) when is_integer(A) -> A end;
            _ -> fun(B) when is_integer(B) -> B end
        end,
    F.

%% Different funs join as one fun of their joined results.
%% expect: different_funs(_) -> fun((_) -> integer() | {_})
different_funs(X) ->
    F =
        case X of
            1 -> fun(A) when is_integer(A) -> A end;
            _ -> fun(B) -> {B} end
        end,
    F.

%% A clause that always raises returns none().
%% expect: raising(0) -> none(); (N) -> N
raising(0) -> error(zero);
raising(N) -> N.
