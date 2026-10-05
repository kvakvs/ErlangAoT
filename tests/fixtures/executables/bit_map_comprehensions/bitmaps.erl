-module(bitmaps).
-export([main/1]).

%% The first argument selects a scenario; each comprehension result is displayed.
main(["producers"]) ->
    L = id([1, 2, 3]),
    B = id(<<1, 2, 3>>),
    M = id(#{1 => 10, 2 => 20, 3 => 30}),
    show([X || X <- L]),
    show(<<<<X>> || X <- L>>),
    show(<<<<X:3>> || X <- L>>),
    show(#{X => X * X || X <- L}),
    show([X || <<X>> <= B]),
    show(<<<<X, X>> || <<X>> <= B>>),
    show(#{X => -X || <<X>> <= B}),
    show([{K, V} || K := V <- M]),
    show(<<<<K, V>> || K := V <- M>>),
    show(#{V => K || K := V <- M}),
    show(#{X rem 3 => X || X <- id([1, 2, 3, 4, 5, 6])}),
    show(#{X => a, -X => b || X <- id([1, 2])}),
    show(<<(id(<<X:4>>)) || X <- L>>),
    show(<<<<X:4>> || <<X:4>> <= id(<<1, 2, 3:4>>)>>),
    show(<<<<X>> || X <- id([])>>),
    show(#{K => V || K := V <- id(#{})}),
    show(<<<<X>> || X <- L, X > 5>>),
    show(<<<<B2/binary>> || B2 <- id([<<"ab">>, <<>>, <<"c">>])>>);
main(["binaries"]) ->
    show([X || <<0:1, X:7>> <= id(<<1, 200, 3>>)]),
    show([X || <<X:3>> <= id(<<255, 1>>)]),
    show([X || <<X:3>> <:= id(<<255, 7:7>>)]),
    show([{S, X} || <<S, X:S>> <= id(<<4, 9:4, 8, 200>>)]),
    show([X || <<X/utf8>> <= id(<<"a", 255, "b">>)]),
    show([{X} || <<X/utf8>> <= id(<<"h", 233/utf8, "llo">>)]),
    show([X || <<"ab", X>> <= id(<<"abcxyabd">>)]),
    show([X || <<1, X:2/binary>> <= id(<<1, 2, 3, 4, 5, 6, 1, 7, 8>>)]),
    show([X || <<X, X>> <= id(<<1, 1, 2, 3, 4, 4>>)]),
    show([X || <<_:4, X:4>> <= id(<<16#1f, 16#2e>>), X > 14]),
    show([X || <<X:16/float>> <= id(<<60, 0, 124, 0, 60, 0>>)]),
    show([X || <<X:32/little>> <= id(<<1, 0, 0, 0, 2, 0, 0, 0>>)]),
    show([X || <<X>> <= id(<<1, 2:4>>)]),
    show([{X, Y} || <<X>> <= id(<<1, 2>>), <<Y>> <= id(<<X, 9>>)]);
main(["maps"]) ->
    M = id(#{3 => c, 1 => a, 2 => b}),
    show([{K, V} || K := V <- M]),
    show([K || K := b <- M]),
    show([K || K := V <- M, V =/= b]),
    show([K || {K, _} := _ <- id(#{{1, a} => a, 2 => b, {3, c} => c})]),
    show([K || K := _ <:- M]),
    show([{K1, K2} || K1 := _ <- id(#{1 => x, 2 => y}), K2 := _ <- id(#{K1 => z})]),
    show(#{K => V + 1 || K := V <- id(#{1 => 1, 2 => 2})}),
    show([V || _ := [V | _] <- id(#{1 => [a], 2 => [], 3 => [c, d]})]);
main(["zips"]) ->
    show([{X, Y} || X <- id([1, 2]) && <<Y>> <= id(<<5, 6, 7:4>>)]),
    show([{X, K, V} || X <- id([a, b]) && K := V <- id(#{1 => x, 2 => y})]),
    show([{X, Y} || <<X:4>> <= id(<<1, 2>>) && <<Y:4>> <= id(<<3, 4>>)]),
    show([{K, X} || {K, _} := _ <- id(#{{1, a} => a, 2 => b, {3, c} => c}) && X <- id([x, y, z])]),
    show(<<<<X, Y>> || X <- id([1, 2]) && <<Y>> <:= id(<<3, 4>>)>>),
    show(#{X => Y || X <- id([1, 2]) && Y <- id([a, b])});
main(["errors"]) ->
    show(caught(binary_input)),
    show(caught(strict_binary_input)),
    show(caught(map_input)),
    show(caught(list_as_map)),
    show(caught(strict_binary)),
    show(caught(strict_binary_tail)),
    show(caught(strict_map)),
    show(caught(strict_float)),
    show(caught(template)),
    show(caught(map_exhausted)),
    show(caught(map_iterator)),
    show(caught(map_strict_zip)),
    show(caught(binary_zip_input)),
    show(caught(binary_zip_strict)),
    show(caught(map_tail));
main(["order"]) ->
    show(#{trace({k, X}) => trace({v, X}) || X <- id([1, 2])}),
    show(caught(trace_template)),
    show(<<<<(trace(X))>> || <<X>> <= id(<<1, 2>>), trace({filter, X}) > 0>>);
main(["uncaught"]) ->
    show(<<X || X <- id([1])>>).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

trace(Value) ->
    show(Value),
    Value.

%% Each scenario raises from inside a comprehension; the error is caught here.
caught(Name) ->
    try run(Name) of
        Value -> {value, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

run(binary_input) -> [X || <<X>> <= id(foo)];
run(strict_binary_input) -> [X || <<X>> <:= id(foo)];
run(map_input) -> [K || K := _ <- id(foo)];
run(list_as_map) -> [K || K := _ <- id([a])];
run(strict_binary) -> [X || <<0:1, X:7>> <:= id(<<1, 200, 3>>)];
run(strict_binary_tail) -> [X || <<X>> <:= id(<<1, 2:4>>)];
run(strict_map) -> [K || K := 1 <:- id(#{1 => 1, 2 => 2})];
run(strict_float) -> [X || <<X:16/float>> <:= id(<<60, 0, 124, 0>>)];
run(template) -> <<X || X <- id([1, 2])>>;
run(map_exhausted) -> [{X, K} || X <- id([1, 2]) && K := _ <- id(#{1 => a})];
run(map_iterator) -> [{K, X} || K := _ <- id(#{1 => a, 2 => b, 3 => c}) && X <- id([x])];
run(map_strict_zip) -> [{K, X} || {K, _} := _ <:- id(#{{1, a} => a, 2 => b}) && X <- id([x, y])];
run(binary_zip_input) -> [{X, Y} || X <- id([1]) && <<Y>> <= id(foo)];
run(binary_zip_strict) -> [{X, Y} || <<X>> <:= id(<<1, 2:4>>) && Y <- id([a, b])];
run(map_tail) -> #{X => 1 || X <- id([1 | t])};
run(trace_template) -> <<(trace(X)) || X <- id([<<1>>, 2, <<3>>])>>.
