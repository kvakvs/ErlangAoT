-module(identities).
-export([main/1]).
-compile(nowarn_deprecated_catch).

% The kind of a term, read with guard type tests.
kind(X) when is_pid(X) -> pid;
kind(X) when is_reference(X) -> ref;
kind(X) when is_function(X) -> 'fun';
kind(X) when is_number(X) -> number;
kind(X) when is_atom(X) -> atom;
kind(X) when is_tuple(X) -> tuple;
kind(X) when is_map(X) -> map;
kind([]) -> nil;
kind(X) when is_list(X) -> list;
kind(X) when is_binary(X) -> binary.

% Identity text with every digit run replaced by N: the numbers differ from OTP's, the layout must not.
shape([]) -> [];
shape([C | Rest]) when C >= $0, C =< $9 -> [$N | shape(digits(Rest))];
shape([C | Rest]) -> [C | shape(Rest)].

digits([C | Rest]) when C >= $0, C =< $9 -> digits(Rest);
digits(Rest) -> Rest.

% Pair every element with its position.
index(List) -> index(List, 1).

index([], _) -> [];
index([H | T], N) -> [{H, N} | index(T, N + 1)].

% Count adjacent distinct elements of a sorted list.
distinct([]) -> 0;
distinct([_]) -> 1;
distinct([X, X | T]) -> distinct([X | T]);
distinct([_ | T]) -> 1 + distinct(T).

% A head that matches only when both arguments are the same identity.
same(X, X) -> same;
same(_, _) -> different.

id(X) -> X.

% The error reason a fun raises, without its stack.
error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

show(X) -> io:format("~w~n", [X]).

main(_) ->
    P = self(),
    R1 = make_ref(),
    R2 = make_ref(),
    show([
        is_pid(P),
        is_reference(R1),
        is_pid(R1),
        is_reference(P),
        is_function(P),
        is_tuple(R1),
        is_port(P)
    ]),
    show([P =:= self(), P == self(), R1 =:= R1, R1 =/= R2, R1 == R2, same(P, self()), same(R1, R2)]),
    show([shape(pid_to_list(P)), shape(ref_to_list(R1)), pid_to_list(P) =:= pid_to_list(self())]),
    show([
        error_of(fun() -> pid_to_list(R1) end),
        error_of(fun() -> ref_to_list(P) end),
        error_of(fun() -> pid_to_list(id(self)) end)
    ]),
    % Term order: number < atom < reference < fun < pid < tuple < map < nil < list < bitstring.
    Mixed = [<<"b">>, [x], [], #{}, {t}, P, fun main/1, R1, atom, 1.5, 7],
    show([kind(X) || X <- lists:sort(Mixed)]),
    show([kind(X) || X <- lists:sort(lists:reverse(Mixed))]),
    show([R1 < P, P < {}, fun main/1 < P, R1 > zzz, max(P, R1) =:= P, min(P, R1) =:= R1]),
    % References are distinct, totally ordered and usable as map keys.
    Refs = [make_ref() || _ <- lists:seq(1, 200)],
    Sorted = lists:sort(Refs),
    show([distinct(Sorted), lists:sort(lists:reverse(Sorted)) =:= Sorted]),
    Positions = maps:from_list(index(Refs)),
    show([map_size(Positions), maps:get(lists:nth(37, Refs), Positions), maps:find(R1, Positions)]),
    Keys = #{P => self, R1 => first, {P, R1} => pair, [R2] => list},
    show([
        maps:get(self(), Keys),
        maps:get(R1, Keys),
        maps:get({self(), R1}, Keys),
        maps:get([R2], Keys)
    ]),
    show(lists:sort([V || {_, V} <- maps:to_list(Keys)])),
    % Identities survive captures, exceptions and nested structures unchanged.
    F = fun() -> {P, R1} end,
    show([F() =:= {P, R1}, {P, [R1]} =:= {self(), [R1]}, (fun erlang:self/0)() =:= P]),
    {'EXIT', {{bad, Caught}, _}} = (catch error({bad, {P, R2}})),
    show(Caught =:= {P, R2}),
    show([kind(make_ref()), kind(self()), make_ref() =/= make_ref()]).
