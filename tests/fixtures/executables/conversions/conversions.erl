-module(conversions).
-export([main/1]).

%% The conversion builtins with valid, boundary and invalid arguments, called
%% through apply/3, directly and as funs.
main([]) ->
    [erlang:display(outcome(F, A)) || {F, A} <- id(calls())],
    direct(),
    large();
main(["atoms"]) ->
    erlang:display(make_atoms(0, 10000)).

id(X) -> X.

%% One call per line: {ok, Value} or the class and reason it raised.
calls() ->
    Long = lists_duplicate(255, $a),
    [
        {atom_to_list, [abc]},
        {atom_to_list, ['']},
        {atom_to_list, ['hello world']},
        {atom_to_list, ["abc"]},
        {atom_to_list, [1]},
        {list_to_atom, ["abc"]},
        {list_to_atom, [[]]},
        {list_to_atom, [[97, 98]]},
        {list_to_atom, [[$a | b]]},
        {list_to_atom, [[-1]]},
        {list_to_atom, [[16#D800]]},
        {list_to_atom, [[16#110000]]},
        {list_to_atom, [[1.0]]},
        {list_to_atom, [abc]},
        {list_to_atom, [Long ++ [x]]},
        {list_to_atom, [Long ++ "b"]},
        {integer_to_list, [0]},
        {integer_to_list, [-15]},
        {integer_to_list, [1180591620717411303424]},
        {integer_to_list, [-1180591620717411303424]},
        {integer_to_list, [1.0]},
        {integer_to_list, [a]},
        {integer_to_list, [255, 16]},
        {integer_to_list, [-5, 2]},
        {integer_to_list, [123456789, 36]},
        {integer_to_list, [0, 7]},
        {integer_to_list, [1180591620717411303424, 16]},
        {integer_to_list, [-1180591620717411303423, 3]},
        {integer_to_list, [10, 1]},
        {integer_to_list, [10, 37]},
        {integer_to_list, [10, a]},
        {integer_to_list, [a, 10]},
        {list_to_integer, ["123"]},
        {list_to_integer, ["-0"]},
        {list_to_integer, ["+7"]},
        {list_to_integer, ["007"]},
        {list_to_integer, ["0000000000000000000000000"]},
        {list_to_integer, ["123456789012345678901234567890"]},
        {list_to_integer, ["-123456789012345678901234567890"]},
        {list_to_integer, [""]},
        {list_to_integer, ["-"]},
        {list_to_integer, ["+"]},
        {list_to_integer, ["1-"]},
        {list_to_integer, ["--1"]},
        {list_to_integer, ["12a"]},
        {list_to_integer, [" 1"]},
        {list_to_integer, [[$1 | $2]]},
        {list_to_integer, [abc]},
        {list_to_integer, ["ff", 16]},
        {list_to_integer, ["-FF", 16]},
        {list_to_integer, ["zZ", 36]},
        {list_to_integer, ["101", 2]},
        {list_to_integer, ["2", 2]},
        {list_to_integer, ["1", 1]},
        {list_to_integer, ["1", 37]},
        {list_to_integer, ["1", a]},
        {list_to_integer, ["123456789abcdef0123456789abcdef", 16]},
        {float_to_list, [1.5]},
        {float_to_list, [-0.0]},
        {float_to_list, [1.0e300]},
        {float_to_list, [1]},
        {float_to_list, [1.5, []]},
        {float_to_list, [1.0, [{scientific, -1}]]},
        {float_to_list, [1.0, [{scientific, 0}]]},
        {float_to_list, [1.0, [{scientific, 250}]]},
        {float_to_list, [-1.0, [{scientific, 249}]]},
        {float_to_list, [3.14159, [{scientific, 3}]]},
        {float_to_list, [1.0, [{decimals, -1}]]},
        {float_to_list, [1.0, [{decimals, 254}]]},
        {float_to_list, [1.0e20, [{decimals, 0}, compact]]},
        {float_to_list, [100.0, [{decimals, 0}, compact]]},
        {float_to_list, [0.125, [{decimals, 2}]]},
        {float_to_list, [2.5, [{decimals, 0}]]},
        {float_to_list, [-2.5, [{decimals, 0}]]},
        {float_to_list, [0.1, [{decimals, 30}]]},
        {float_to_list, [1.5, [{decimals, 3}, compact]]},
        {float_to_list, [10.0, [{decimals, 3}, compact]]},
        {float_to_list, [-0.0, [{decimals, 2}]]},
        {float_to_list, [0.999, [{decimals, 2}]]},
        {float_to_list, [123456.789, [{decimals, 1}]]},
        {float_to_list, [1.0e300, [{decimals, 2}]]},
        {float_to_list, [1.5, [compact]]},
        {float_to_list, [1.5, [short, {decimals, 2}]]},
        {float_to_list, [1.5, [{decimals, 2}, short]]},
        {float_to_list, [1.5, [short]]},
        {float_to_list, [1.0e10, [short]]},
        {float_to_list, [123456789012345680.0, [short]]},
        {float_to_list, [0.0001, [short]]},
        {float_to_list, [0.00001, [short]]},
        {float_to_list, [100.0, [short]]},
        {float_to_list, [1000.0, [short]]},
        {float_to_list, [-0.0, [short]]},
        {float_to_list, [9007199254740992.0, [short]]},
        {float_to_list, [9007199254740991.0, [short]]},
        {float_to_list, [1.0e-300, [short]]},
        {float_to_list, [5.0e-324, [short]]},
        {float_to_list, [1.7976931348623157e308, [short]]},
        {float_to_list, [0.1, [short]]},
        {float_to_list, [1234.5678, [short]]},
        {float_to_list, [1.0e9, [short]]},
        {float_to_list, [1.2345e-7, [short]]},
        {float_to_list, [1.5, [bad]]},
        {float_to_list, [1.5, x]},
        {float_to_list, [1.5, [{decimals, 1.0}]]},
        {float_to_list, [1.5, [{other, 1}]]},
        {float_to_list, [1.5, [compact | short]]},
        {binary_to_list, [<<>>]},
        {binary_to_list, [<<1, 2, 255>>]},
        {binary_to_list, [<<1:3>>]},
        {binary_to_list, [abc]},
        {list_to_binary, [[]]},
        {list_to_binary, [[1, 2, <<3>>, [4, [5]] | <<6>>]]},
        {list_to_binary, [[[], [[]], <<>>]]},
        {list_to_binary, [<<1>>]},
        {list_to_binary, [[256]]},
        {list_to_binary, [[-1]]},
        {list_to_binary, [[<<1:3>>]]},
        {list_to_binary, [[a]]},
        {list_to_binary, [[1 | 2]]},
        {list_to_binary, [[1 | <<2:4>>]]},
        {iolist_to_binary, [<<1, 2>>]},
        {iolist_to_binary, [[<<"ab">>, $c]]},
        {iolist_to_binary, [<<1:3>>]},
        {iolist_to_binary, [a]}
    ].

%% The value of erlang:F(Args), or the class and reason it raised.
outcome(F, A) ->
    try apply(erlang, F, A) of
        Value -> {ok, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

lists_duplicate(0, _) -> [];
lists_duplicate(N, X) -> [X | lists_duplicate(N - 1, X)].

%% Direct and erlang-qualified calls, funs and round trips.
direct() ->
    N = id(-12345678901234567890),
    erlang:display([
        list_to_integer(integer_to_list(N)) =:= N,
        list_to_integer(erlang:integer_to_list(N, 7), 7) =:= N,
        atom_to_list(list_to_atom(id("round trip"))),
        length(atom_to_list(list_to_atom([955, 1000]))),
        binary_to_list(list_to_binary(id(["ab", <<"cd">>]))),
        iolist_to_binary(id([<<"x">>, ["y", $z]])),
        float_to_list(id(2.5)),
        erlang:float_to_list(id(2.5), [short]),
        [F(X) || {F, X} <- [{fun integer_to_list/1, 42}, {fun erlang:atom_to_list/1, ok}]],
        fun list_to_atom/1 =:= fun erlang:list_to_atom/1
    ]).

%% Large inputs: long digit strings, deep and long iolists.
large() ->
    Digits = lists_duplicate(1000, $7),
    Big = list_to_integer(Digits),
    erlang:display([
        length(integer_to_list(Big)),
        integer_to_list(Big) =:= Digits,
        length(integer_to_list(Big, 2)),
        list_to_integer(integer_to_list(Big, 36), 36) =:= Big,
        byte_size(list_to_binary(nest(100000))),
        byte_size(iolist_to_binary(lists_duplicate(10000, <<"ab">>))),
        length(binary_to_list(list_to_binary(lists_duplicate(10000, 255))))
    ]),
    Ones = binary_to_list(binary_part(double(<<"1">>, 21), 0, 1300000)),
    erlang:display(attempt(fun() -> list_to_integer(Ones) end)),
    erlang:display(attempt(fun() -> list_to_integer([$x | Ones]) end)).

%% Binary doubled N times.
double(Binary, 0) -> Binary;
double(Binary, N) -> double(<<Binary/binary, Binary/binary>>, N - 1).

%% A list nested Depth levels deep around one byte.
nest(0) -> [1];
nest(Depth) -> [nest(Depth - 1)].

%% The value of Fun(), or the class and reason it raised.
attempt(Fun) ->
    try Fun() of
        Value -> {ok, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

%% Create atoms until the table is full; under the default table this returns the count.
make_atoms(N, N) ->
    N;
make_atoms(I, N) ->
    _ = list_to_atom("atom_" ++ integer_to_list(I)),
    make_atoms(I + 1, N).
