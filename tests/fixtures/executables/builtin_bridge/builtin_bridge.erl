-module(builtin_bridge).
-export([main/1, visible/1]).

%% Builtins reached through the bridge: every bridge builtin through apply/3 with
%% valid and invalid arguments, funs of builtins, M:F(Args) naming builtins and
%% erlang:function_exported/3.
main([]) ->
    values(),
    errors(),
    funs(),
    exported();
main(["halt"]) ->
    apply(id(erlang), halt, [id(3)]);
main(["halt0"]) ->
    halt();
main(["uncaught"]) ->
    apply(id(erlang), error, [id(boom)]).

visible(X) -> X.

id(X) -> X.

%% Every bridge builtin with valid arguments, called through apply/3.
values() ->
    Calls = [
        {is_atom, [a]},
        {is_binary, [<<1>>]},
        {is_bitstring, [<<1:1>>]},
        {is_boolean, [true]},
        {is_float, [1.5]},
        {is_function, [fun id/1]},
        {is_function, [fun id/1, 1]},
        {is_integer, [7]},
        {is_list, [[1]]},
        {is_map, [#{}]},
        {is_number, [2.5]},
        {is_pid, [a]},
        {is_port, [a]},
        {is_reference, [a]},
        {is_tuple, [{}]},
        {abs, [-5]},
        {bit_size, [<<1:3>>]},
        {byte_size, [<<1, 2>>]},
        {ceil, [1.2]},
        {element, [2, {a, b}]},
        {float, [3]},
        {floor, [-1.5]},
        {hd, [[h | t]]},
        {length, [[1, 2, 3]]},
        {map_get, [k, #{k => v}]},
        {map_size, [#{1 => 1, 2 => 2}]},
        {is_map_key, [1, #{1 => 1}]},
        {max, [1, 2.0]},
        {min, [b, a]},
        {round, [2.5]},
        {size, [{1, 2}]},
        {tl, [[1, 2]]},
        {trunc, [-2.7]},
        {tuple_size, [{1, 2, 3}]},
        {binary_part, [<<"hello">>, {1, 3}]},
        {binary_part, [<<"hello">>, 0, 2]},
        {'=:=', [1, 1.0]},
        {'=/=', [1, 1.0]},
        {'==', [1, 1.0]},
        {'/=', [a, b]},
        {'<', [1, a]},
        {'=<', [2, 2]},
        {'>', [[a], {}]},
        {'>=', [3, 4]},
        {'not', [true]},
        {'and', [true, false]},
        {'or', [false, true]},
        {'xor', [true, true]},
        {'+', [1, 2]},
        {'-', [10, 4]},
        {'*', [6, 7]},
        {'/', [7, 2]},
        {'div', [7, 2]},
        {'rem', [-7, 2]},
        {'band', [12, 10]},
        {'bor', [12, 10]},
        {'bxor', [12, 10]},
        {'bsl', [1, 70]},
        {'bsr', [-16, 2]},
        {'+', [5]},
        {'-', [5]},
        {'bnot', [5]},
        {display, [shown]},
        {raise, [bad, x, []]}
    ],
    erlang:display([apply(erlang, F, A) || {F, A} <- id(Calls)]).

%% Invalid arguments raise the errors the builtins raise when called directly.
errors() ->
    Calls = [
        {abs, [a]},
        {element, [3, {a}]},
        {hd, [[]]},
        {tl, [x]},
        {length, [[1 | 2]]},
        {tuple_size, [x]},
        {size, [1]},
        {float, [x]},
        {round, [x]},
        {trunc, [x]},
        {floor, [x]},
        {ceil, [x]},
        {bit_size, [x]},
        {byte_size, [x]},
        {map_get, [k, #{}]},
        {map_get, [k, x]},
        {map_size, [x]},
        {is_map_key, [k, x]},
        {is_function, [x, -1]},
        {binary_part, [<<"ab">>, {1, 5}]},
        {binary_part, [<<"ab">>, x]},
        {binary_part, [<<"ab">>, 3, 1]},
        {'not', [1]},
        {'and', [1, true]},
        {'or', [x, y]},
        {'xor', [1, 2]},
        {'+', [a, 1]},
        {'-', [a]},
        {'/', [1, 0]},
        {'div', [1, 0]},
        {'rem', [1.0, 2]},
        {'bsl', [1, a]},
        {'bsl', [1, 5000000]},
        {'bnot', [1.0]},
        {error, [boom]},
        {error, [boom, [1]]},
        {error, [boom, [1], []]},
        {exit, [bye]},
        {throw, [ball]},
        {raise, [throw, ball, []]},
        {raise, [exit, gone, []]},
        {halt, [x]},
        {function_exported, [1, f, 1]},
        {function_exported, [m, 1, 1]},
        {function_exported, [m, f, x]},
        {no_such_builtin, [1]}
    ],
    erlang:display([outcome(F, A) || {F, A} <- id(Calls)]).

%% The value of erlang:F(Args), or the class and reason it raised.
outcome(F, A) ->
    try apply(erlang, F, A) of
        Value -> {ok, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

%% Builtins as fun values: fun F/A of an auto-imported builtin is fun erlang:F/A.
funs() ->
    Abs = fun abs/1,
    erlang:display([Abs(-2), (fun erlang:abs/1)(-3), Abs =:= fun erlang:abs/1, Abs]),
    erlang:display([fun erlang:'+'/2, fun is_atom/1, fun erlang:display/1, fun halt/1]),
    erlang:display(map(fun tuple_size/1, [{}, {a}, {a, b}])),
    erlang:display(map(fun erlang:'-'/1, [1, -2])),
    erlang:display(fold(fun erlang:max/2, 0, [3, 9, 2])),
    erlang:display(fold(fun erlang:'+'/2, 0, [1, 2, 3, 4])),
    erlang:display([X || X <- [a, 1, b, 2.0], (fun is_atom/1)(X)]),
    M = id(erlang),
    N = id(element),
    Element = fun M:N/2,
    erlang:display([Element(1, {x}), Element =:= fun erlang:element/2, M:N(2, {y, z}), M:'-'(9, 4)]),
    erlang:display([fold(fun M:min/2, 10, [4, 7]), apply(fun erlang:hd/1, [[q]])]),
    erlang:display(outcome2(fun erlang:abs/1, [1, 2])),
    erlang:display(outcome2(fun erlang:throw/1, [thrown])).

%% The value of F(Args), or the class and reason it raised.
outcome2(F, A) ->
    try apply(F, A) of
        Value -> {ok, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

map(F, [H | T]) -> [F(H) | map(F, T)];
map(_, []) -> [].

fold(F, Acc, [H | T]) -> fold(F, F(H, Acc), T);
fold(_, Acc, []) -> Acc.

%% erlang:function_exported/3 is true for exported functions of the program and for builtins.
exported() ->
    erlang:display([
        erlang:function_exported(builtin_bridge, visible, 1),
        erlang:function_exported(builtin_bridge, visible, 2),
        erlang:function_exported(builtin_bridge, id, 1),
        erlang:function_exported(erlang, abs, 1),
        erlang:function_exported(erlang, '+', 2),
        erlang:function_exported(erlang, display, 1),
        erlang:function_exported(id(nomodule), f, 0),
        erlang:function_exported(builtin_bridge, visible, id(-1))
    ]).
