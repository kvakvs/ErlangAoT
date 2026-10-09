%% Narrowing expectations (tests/compiler/inference/expectations.py, docs/semantic.md#inference-expectations): patterns,
%% guards and type tests narrow the facts of the values they test within their clause, and a function's inputs are
%% its entry domain, the join over its clauses of each argument's narrowed fact.
-module(narrowing).
-export([
    atom_test/1,
    boolean_test/1,
    integer_test/1,
    float_test/1,
    number_test/1,
    binary_test/1,
    bitstring_test/1,
    list_test/1,
    tuple_test/1,
    map_test/1,
    function_test/1,
    function_arity_test/1,
    pid_test/1,
    port_test/1,
    reference_test/1,
    record_test/1,
    record_size_test/1,
    map_key_test/2,
    legacy_test/1,
    case_guard/1,
    case_test/1,
    andalso_test/1,
    filter_test/1,
    tuple_pattern/1,
    list_pattern/1,
    map_pattern/1,
    catch_all/1,
    small/1,
    contradiction/1,
    either/1,
    complement_start/0,
    after_call/1,
    no_leak/1,
    no_leak_andalso/1,
    below/1,
    at_most/1,
    above/1,
    equal_to/1,
    between/2,
    nonzero/1,
    not_five/1,
    classify/1,
    case_range/1,
    if_range/1,
    either_range/1,
    loop_start/0,
    number_compare/1,
    term_compare/1
]).

-record(point, {x, y}).

%% A type test in a function guard narrows its argument to the test's category.

%% expect: atom_test(atom()) -> atom()
atom_test(X) when is_atom(X) -> X.

%% expect: boolean_test(boolean()) -> boolean()
boolean_test(X) when is_boolean(X) -> X.

%% expect: integer_test(integer()) -> integer()
integer_test(X) when is_integer(X) -> X.

%% expect: float_test(float()) -> float()
float_test(X) when is_float(X) -> X.

%% expect: number_test(number()) -> number()
number_test(X) when is_number(X) -> X.

%% expect: binary_test(binary()) -> binary()
binary_test(X) when is_binary(X) -> X.

%% expect: bitstring_test(bitstring()) -> bitstring()
bitstring_test(X) when is_bitstring(X) -> X.

%% expect: list_test(maybe_improper_list()) -> maybe_improper_list()
list_test(X) when is_list(X) -> X.

%% expect: tuple_test(tuple()) -> tuple()
tuple_test(X) when is_tuple(X) -> X.

%% expect: map_test(map()) -> map()
map_test(X) when is_map(X) -> X.

%% expect: function_test(fun()) -> fun()
function_test(X) when is_function(X) -> X.

%% expect: function_arity_test(fun((_, _) -> _)) -> fun((_, _) -> _)
function_arity_test(X) when is_function(X, 2) -> X.

%% expect: pid_test(pid()) -> pid()
pid_test(X) when is_pid(X) -> X.

%% expect: port_test(port()) -> port()
port_test(X) when is_port(X) -> X.

%% expect: reference_test(reference()) -> reference()
reference_test(X) when is_reference(X) -> X.

%% expect: record_test({point, _, _}) -> {point, _, _}
record_test(X) when is_record(X, point) -> X.

%% expect: record_size_test({point, _, _}) -> {point, _, _}
record_size_test(X) when is_record(X, point, 3) -> X.

%% expect: map_key_test(_, map()) -> map()
map_key_test(K, M) when is_map_key(K, M) -> M.

%% The old guard names narrow as their is_ forms.
%% expect: legacy_test(integer()) -> integer()
legacy_test(X) when integer(X) -> X.

%% Case guards, a true test as the scrutinee, andalso conditions and comprehension filters narrow too.

%% expect: case_guard(_) -> integer() | other
case_guard(X) ->
    case X of
        Y when is_integer(Y) -> Y;
        _ -> other
    end.

%% expect: case_test(_) -> 0 | atom()
case_test(X) ->
    case is_atom(X) of
        true -> X;
        false -> 0
    end.

%% expect: andalso_test(_) -> integer() | false
andalso_test(X) -> is_integer(X) andalso X + 1.

%% expect: filter_test(_) -> [integer()]
filter_test(L) -> [X + 1 || X <- L, is_integer(X)].

%% Patterns narrow what they match; a catch-all clause makes the domain term().

%% expect: tuple_pattern({ok, _}) -> _
tuple_pattern({ok, V}) -> V.

%% expect: list_pattern([_, ...]) -> _
list_pattern([H]) -> H.

%% expect: map_pattern(map()) -> _
map_pattern(#{a := V}) -> V.

%% expect: catch_all(_) -> one | other
catch_all(1) -> one;
catch_all(_) -> other.

%% Comparisons narrow a value proven to be an integer to a range.
%% expect: small(integer()) -> big | small
small(X) when is_integer(X), X > 0, X < 10 -> small;
small(X) when is_integer(X) -> big.

%% A test that contradicts what is known makes its clause impossible.
%% expect: contradiction(_) -> ok
contradiction(X) when is_integer(X), X >= 1, X =< 10, is_atom(X) -> never;
contradiction(_) -> ok.

%% A disjunction narrows to the join of its alternatives.
%% expect: either(number()) -> number()
either(X) when is_integer(X); is_float(X) -> X.

%% A clause after one whose whole guard was a single type test sees the value without that category.
%% expect: complement_start() -> float() | int
complement_start() ->
    {complement(1), complement(2.5)},
    complement(3).

%% expect: complement(1 | 3 | float()) -> float() | int
complement(X) when is_integer(X) -> int;
complement(X) -> X.

%% A call that returns narrows its argument variables to the callee's entry domain.
%% expect: after_call(non_neg_integer()) -> non_neg_integer()
after_call(X) ->
    _ = natural(X),
    X.

%% expect: natural(non_neg_integer()) -> non_neg_integer()
natural(Y) when is_integer(Y), Y >= 0 -> Y.

%% Narrowings never leak out of the clause or operand that proved them.
%% expect: no_leak(X) -> X
no_leak(X) ->
    _ =
        case X of
            Y when is_integer(Y) -> Y;
            _ -> 0
        end,
    X.

%% expect: no_leak_andalso(X) -> X
no_leak_andalso(X) ->
    _ = is_integer(X) andalso X,
    X.

%% Comparisons narrow a proven integer to the range they imply, the constant on either side.

%% expect: below(neg_integer()) -> neg_integer()
below(X) when is_integer(X), X < 0 -> X.

%% expect: at_most(0..10) -> 0..10
at_most(X) when is_integer(X), 10 >= X, X >= 0 -> X.

%% expect: above(pos_integer()) -> pos_integer()
above(X) when is_integer(X), 0 < X -> X.

%% expect: equal_to(5) -> 5
equal_to(X) when is_integer(X), X == 5 -> X.

%% Two proven integers narrow each other by their bounds.
%% expect: between(pos_integer(), 0..5) -> pos_integer()
between(X, Y) when is_integer(X), is_integer(Y), Y >= 0, Y =< 5, X > Y -> X.

%% Excluding a range's bound moves it inward; excluding a value inside it proves nothing.
%% expect: nonzero(1..10) -> 1..10
nonzero(X) when is_integer(X), X >= 0, X =< 10, X =/= 0 -> X.

%% expect: not_five(0..10) -> 0..10
not_five(X) when is_integer(X), X >= 0, X =< 10, X =/= 5 -> X.

%% A clause after one whose whole guard was a single comparison sees it false.
%% expect: classify(_) -> neg_integer() | positive
classify(N) when N >= 0 -> positive;
classify(N) when is_integer(N) -> N.

%% expect: case_range(_) -> 0..3
case_range(X) ->
    case X of
        Y when is_integer(Y), Y > 0, Y < 4 -> Y;
        _ -> 0
    end.

%% expect: if_range(integer()) -> 0..3
if_range(X) when is_integer(X) ->
    if
        X > 0, X < 4 -> X;
        true -> 0
    end.

%% expect: either_range(1 | 3) -> 1 | 3
either_range(X) when is_integer(X), X >= 0, X =< 10, (X == 1 orelse X == 3) -> X.

%% A guarded countdown keeps the bound its guards prove.
%% expect: loop_start() -> done
loop_start() -> loop(10).

%% expect: loop(0..10) -> done
loop(N) when N > 0 -> loop(N - 1);
loop(0) -> done.

%% A value that may be a float, or any term, does not narrow.
%% expect: number_compare(number()) -> number()
number_compare(X) when is_number(X), X > 0 -> X.

%% expect: term_compare(X) -> X
term_compare(X) when X > 0 -> X.
