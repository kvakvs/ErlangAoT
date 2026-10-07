-module(funvals).
-export([main/1, add/2, count/2, inc/1]).
-compile(nowarn_deprecated_catch).

add(X, Y) -> X + Y.

private(X) -> {private, X}.

negate(X) -> -X.

inc(X) -> X + 1.

% Many fun values alive at once, built and applied across heap collections.
funs(0) -> [];
funs(N) -> [choose(N rem 3) | funs(N - 1)].

choose(0) -> fun negate/1;
choose(1) -> fun funpeer:double/1;
choose(2) -> fun funvals:inc/1.

step(F, Acc) -> F(Acc) rem 1000003.

count(0, Acc) -> Acc;
count(N, Acc) -> count(N - 1, Acc + 1).

% Higher-order helpers written with fun values only.
map(_, []) -> [];
map(F, [H | T]) -> [F(H) | map(F, T)].

fold(_, Acc, []) -> Acc;
fold(F, Acc, [H | T]) -> fold(F, F(H, Acc), T).

% A tail call through a fun value runs in constant stack.
loop(_, 0, Acc) -> Acc;
loop(F, N, Acc) -> loop(F, N - 1, F(Acc, 1)).

% Body recursion that calls through a fun at every level.
depth(_, 0) -> 0;
depth(F, N) -> F(depth(F, N - 1), 1).

tail_apply(F, X) -> F(X).

caught(F, Args) ->
    case catch apply_list(F, Args) of
        {'EXIT', {{badarity, {Fun, Given}}, _}} -> {badarity, Fun =:= F, Given};
        {'EXIT', {{badfun, Value}, _}} -> {badfun, Value};
        {'EXIT', {Reason, _}} -> {error, Reason};
        Value -> {ok, Value}
    end.

apply_list(F, []) -> F();
apply_list(F, [A]) -> F(A);
apply_list(F, [A, B]) -> F(A, B);
apply_list(F, [A, B, C]) -> F(A, B, C).

values() ->
    Local = fun negate/1,
    Again = fun negate/1,
    Remote = fun funpeer:double/1,
    Self = fun funvals:add/2,
    Unknown = fun nowhere:thing/0,
    erlang:display([Remote, Self, Unknown, fun 'odd name':'f g'/3]),
    erlang:display([Local =:= Again, Local == Again, Local =:= Remote, Self =:= funpeer:pick(add)]),
    erlang:display([
        is_function(Local),
        is_function(Local, 1),
        is_function(Local, 2),
        is_function(Remote, 1),
        is_function(Unknown, 0),
        is_function(add),
        is_function({}, 0)
    ]),
    Kinds = [
        if
            is_function(X, 1) -> one;
            is_function(X) -> other;
            true -> none
        end
     || X <- [Local, Self, Unknown, 7]
    ],
    erlang:display(Kinds),
    erlang:display(lists_sorted([Unknown, Remote, 3, Self, {t}, a])),
    erlang:display([Local < Remote, Remote < Self, Self < {}, a < Local, Local < [], Unknown > Self]),
    erlang:display(private(1)).

% A small insertion sort: values compare across kinds.
lists_sorted(List) -> fold(fun insert/2, [], List).

insert(X, []) -> [X];
insert(X, [H | T]) when X =< H -> [X, H | T];
insert(X, [H | T]) -> [H | insert(X, T)].

calls() ->
    erlang:display((fun negate/1)(5)),
    erlang:display((fun funvals:add/2)(2, 3)),
    erlang:display(funpeer:apply_twice(fun funpeer:double/1, 3)),
    erlang:display((funpeer:hidden_caller())(x)),
    erlang:display(map(fun negate/1, [1, 2, 3])),
    erlang:display(fold(fun add/2, 0, [1, 2, 3, 4])),
    Table = #{neg => fun negate/1, dbl => funpeer:pick(double)},
    erlang:display([(maps_get(neg, Table))(4), (maps_get(dbl, Table))(4)]),
    {First, Second} = {fun negate/1, fun add/2},
    erlang:display({First(1), Second(1, 2)}),
    erlang:display(loop(fun add/2, 100000, 0)),
    erlang:display(depth(fun add/2, 20000)),
    erlang:display(tail_apply(fun negate/1, 7)),
    erlang:display(fold(fun step/2, 1, funs(30000))).

maps_get(Key, Map) ->
    #{Key := Value} = Map,
    Value.

errors() ->
    erlang:display(caught(fun negate/1, [1, 2])),
    erlang:display(caught(fun funpeer:double/1, [])),
    erlang:display(caught(fun nowhere:thing/0, [1])),
    erlang:display(caught(fun nowhere:thing/0, [])),
    erlang:display(caught(funpeer:pick(missing), [1])),
    erlang:display(caught(funpeer:pick(private), [1])),
    erlang:display(caught(funpeer:pick(elsewhere), [])),
    erlang:display(caught(add, [1])),
    erlang:display(caught({a, b}, [])),
    erlang:display(caught(42, [1, 2, 3])),
    erlang:display(caught(fun funpeer:double/1, [x])).

main([]) ->
    values(),
    calls(),
    errors();
main(["badfun"]) ->
    apply_list(nope, [1]);
main(["badarity"]) ->
    apply_list(fun funpeer:double/1, [1, 2]);
main(["undef"]) ->
    apply_list(fun nowhere:thing/0, []).
