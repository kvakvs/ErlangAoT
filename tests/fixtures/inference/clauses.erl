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
    raising/1,
    selected/0,
    first_branch/0,
    unknown_argument/1,
    no_type/0,
    narrowed_after/1,
    literal_argument/0,
    overlapping/1,
    below_ten/0,
    nested_calls/0,
    several_callers/0,
    recursive_call/0,
    bound_fun/0,
    fun_in_tuple/0,
    passed_fun/0,
    unknown_fun_argument/1,
    per_call/0,
    local_shape/1,
    deep_enough/0,
    too_deep/0,
    hop1/1,
    hop2/1,
    hop3/1,
    hop4/1,
    step1/1,
    step2/1,
    step3/1,
    step4/1,
    step5/1,
    large_callee/0,
    large/1,
    recursive_callee/0,
    wrap_n/2,
    unknown_callee/1
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

%% A nested case splits further (docs/semantic.md#dependent-facts).
%% expect: nested_case(1, spanish) -> uno; (1, _) -> one; (_, _) -> other
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

%% A case that does not end the body splits it through the values that depend on it.
%% expect: not_last(1) -> {one}; (_) -> {other}
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
        case X > 0 of
            true -> fun(A) when is_integer(A) -> A end;
            false -> fun(B) -> {B} end
        end,
    F.

%% A clause that always raises returns none().
%% expect: raising(0) -> none(); (N) -> N
raising(0) -> error(zero);
raising(N) -> N.

%% Calls read the function types their arguments select (step 58L), then evaluate the callee again with their
%% argument facts (step 58M).
%% expect: selected() -> 6
selected() -> type_tests(5).

%% An exact clause whose inputs hold the arguments hides the later ones.
%% expect: first_branch() -> one
first_branch() -> case_split(1).

%% Unknown arguments select every function type: the union.
%% expect: unknown_argument(_) -> integer() | one | other
unknown_argument(X) -> case_split(X).

%% A call no function type admits never returns.
%% expect: no_type() -> none()
no_type() -> type_tests(1.5).

%% After the call, the argument narrows to the inputs of the types it entered.
%% expect: narrowed_after(integer()) -> integer()
narrowed_after(X) when is_number(X) ->
    _ = type_tests(X),
    X.

%% expect: literal_argument() -> two
literal_argument() -> literals(2).

%% A range overlapping two clauses selects both.
%% expect: overlapping(0..20) -> big | small
overlapping(X) when is_integer(X), X >= 0, X =< 20 -> small_big(X).

%% expect: below_ten() -> small
below_ten() -> small_big(5).

%% A plain variable after a single comparison sees it false (58H1).
%% expect: small_big(0..9) -> small; (10..20) -> big
small_big(N) when N < 10 -> small;
small_big(N) -> big.

%% expect: nested_calls() -> 3
nested_calls() -> type_tests(type_tests(1)).

%% A local function's inputs join its callers' arguments; each call still selects by its own.
%% expect: several_callers() -> {one, other}
several_callers() -> {pick_local(1), pick_local(a)}.

%% expect: pick_local(1) -> one; (1 | a) -> other
pick_local(1) -> one;
pick_local(_) -> other.

%% expect: recursive_call() -> done
recursive_call() -> countdown(3).

%% A bound fun evaluated for a call enters only the clauses its arguments can match.
%% expect: bound_fun() -> other
bound_fun() ->
    F = fun
        (1) -> one;
        (X) when is_atom(X) -> X;
        (_) -> other
    end,
    F(2).

%% Funs elsewhere select by their function types.
%% expect: fun_in_tuple() -> other
fun_in_tuple() ->
    {F} = {
        fun
            (1) -> one;
            (_) -> other
        end
    },
    F(2).

%% expect: passed_fun() -> other
passed_fun() ->
    apply_two(fun
        (1) -> one;
        (_) -> other
    end).

%% expect: apply_two(fun((1) -> one; (_) -> other)) -> other
apply_two(F) -> F(2).

%% expect: unknown_fun_argument(_) -> one | other
unknown_fun_argument(X) ->
    F = fun
        (1) -> one;
        (_) -> other
    end,
    F(X).

%% Each call evaluates its callee again with its own argument facts (step 58M).
%% expect: per_call() -> {3, {a}}
per_call() -> {local_shape(3), local_shape(a)}.

%% expect: local_shape(integer()) -> integer(); (_) -> {_}
local_shape(X) when is_integer(X) -> X;
local_shape(X) -> {X}.

%% Re-analyses nest at most 4 deep: past that a call uses the function types.
%% expect: deep_enough() -> {a}
deep_enough() -> hop1(a).

%% expect: hop1(_) -> {_}
hop1(X) -> hop2(X).

%% expect: hop2(_) -> {_}
hop2(X) -> hop3(X).

%% expect: hop3(_) -> {_}
hop3(X) -> hop4(X).

%% expect: hop4(_) -> {_}
hop4(X) -> {X}.

%% expect: too_deep() -> {_}
too_deep() -> step1(a).

%% expect: step1(_) -> {_}
step1(X) -> step2(X).

%% expect: step2(_) -> {_}
step2(X) -> step3(X).

%% expect: step3(_) -> {_}
step3(X) -> step4(X).

%% expect: step4(_) -> {_}
step4(X) -> step5(X).

%% expect: step5(_) -> {_}
step5(X) -> {X}.

%% A callee of more than 256 expressions is not evaluated again.
%% expect: large_callee() -> {_}
large_callee() -> large(a).

%% expect: large(_) -> {_}
large(X) ->
    _ = [
        [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        [10, 11, 12, 13, 14, 15, 16, 17, 18, 19],
        [20, 21, 22, 23, 24, 25, 26, 27, 28, 29],
        [30, 31, 32, 33, 34, 35, 36, 37, 38, 39],
        [40, 41, 42, 43, 44, 45, 46, 47, 48, 49],
        [50, 51, 52, 53, 54, 55, 56, 57, 58, 59],
        [60, 61, 62, 63, 64, 65, 66, 67, 68, 69],
        [70, 71, 72, 73, 74, 75, 76, 77, 78, 79],
        [80, 81, 82, 83, 84, 85, 86, 87, 88, 89],
        [90, 91, 92, 93, 94, 95, 96, 97, 98, 99],
        [100, 101, 102, 103, 104, 105, 106, 107, 108, 109],
        [110, 111, 112, 113, 114, 115, 116, 117, 118, 119],
        [120, 121, 122, 123, 124, 125, 126, 127, 128, 129],
        [130, 131, 132, 133, 134, 135, 136, 137, 138, 139],
        [140, 141, 142, 143, 144, 145, 146, 147, 148, 149],
        [150, 151, 152, 153, 154, 155, 156, 157, 158, 159],
        [160, 161, 162, 163, 164, 165, 166, 167, 168, 169],
        [170, 171, 172, 173, 174, 175, 176, 177, 178, 179],
        [180, 181, 182, 183, 184, 185, 186, 187, 188, 189],
        [190, 191, 192, 193, 194, 195, 196, 197, 198, 199],
        [200, 201, 202, 203, 204, 205, 206, 207, 208, 209],
        [210, 211, 212, 213, 214, 215, 216, 217, 218, 219],
        [220, 221, 222, 223, 224, 225, 226, 227, 228, 229],
        [230, 231, 232, 233, 234, 235, 236, 237, 238, 239]
    ],
    {X}.

%% A recursive callee is not evaluated again.
%% expect: recursive_callee() -> {_}
recursive_callee() -> wrap_n(2, a).

%% expect: wrap_n(0, _) -> {_}; (_, _) -> {_}
wrap_n(0, X) -> {X};
wrap_n(N, X) -> wrap_n(N - 1, X).

%% Unknown arguments are no narrower than the callee's inputs: the function types decide.
%% expect: unknown_callee(_) -> integer() | {_}
unknown_callee(X) -> local_shape(X).
