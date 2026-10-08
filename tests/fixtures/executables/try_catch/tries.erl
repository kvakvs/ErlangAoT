-module(tries).
-export([main/1]).
-compile(nowarn_deprecated_catch).

-record(pt, {x, y}).

%% The first argument selects a scenario; each try result is displayed.
main(["classes"]) ->
    show(
        try
            throw(ball)
        catch
            throw:B -> {thrown, B}
        end
    ),
    show(
        try
            throw(plain)
        catch
            P -> {default_class, P}
        end
    ),
    show(
        try
            error(oops)
        catch
            error:E -> {error, E}
        end
    ),
    show(
        try
            exit(shutdown)
        catch
            exit:X -> {exit, X}
        end
    ),
    show(
        try
            erlang:error(with_args, [a])
        catch
            error:W -> {error, W}
        end
    ),
    show(
        try
            id(value)
        catch
            _:_ -> unreachable
        end
    ),
    show(class_of(fun_throw)),
    show(class_of(fun_error)),
    show(class_of(fun_exit)),
    show(class_of(fun_none));
main(["runtime"]) ->
    show(runtime(fun_badmatch)),
    show(runtime(fun_clause)),
    show(runtime(fun_badarith)),
    show(runtime(fun_case)),
    show(runtime(fun_if)),
    show(runtime(fun_badarg)),
    show(runtime(fun_badkey)),
    show(runtime(fun_badmap)),
    show(runtime(fun_badrecord)),
    show(runtime(fun_record));
main(["select"]) ->
    show(select(id({tag, 1}))),
    show(select(id({tag, -1}))),
    show(select(id({other, 2}))),
    show(select(id(#{key => 3}))),
    show(select(id(#pt{x = 4, y = 5}))),
    show(select(id(<<6, 7>>))),
    show(select(id(last))),
    show(bound(id(k), id(k))),
    show(bound(id(k), id(j))),
    show(same_atom(id(throw))),
    show(same_atom(id(error))),
    show(guarded(id(5))),
    show(guarded(id(-5))),
    show(guarded(id(zero)));
main(["of"]) ->
    show(sign(id(3))),
    show(sign(id(-3))),
    show(sign(id(0))),
    show(catch sign(id(nan))),
    show(of_scope(id(10))),
    show(catch of_raises(id(1))),
    show(catch handler_raises(id(1))),
    show(of_value(id(2)));
main(["nested"]) ->
    show(
        try
            try
                throw(inner)
            catch
                error:_ -> wrong
            end
        catch
            C1:R1 -> {outer, C1, R1}
        end
    ),
    show(
        try
            try
                error(inner)
            catch
                error:I -> throw({rethrown, I})
            end
        catch
            throw:T -> {outer, T}
        end
    ),
    show(
        try
            try id(1) of
                1 -> exit(from_of)
            catch
                exit:_ -> not_here
            end
        catch
            exit:O -> {outer, O}
        end
    ),
    show(
        try
            raiser:depth(3)
        catch
            throw:Deep -> {caught, Deep}
        end
    ),
    show(
        try
            raiser:depth(0)
        catch
            throw:_ -> no
        end
    ),
    show(
        try
            raiser:unmatched(id(b))
        catch
            error:U -> {unmatched, U}
        end
    ),
    show(catch raiser:unmatched(id(a))),
    show(
        try
            raiser:passthrough(id(2))
        catch
            exit:Pass -> {passed, Pass}
        end
    ),
    show(
        catch (try id(7) of
            8 -> eight
        catch
            _ -> c
        end)
    );
main(["flow"]) ->
    Before = id(before),
    R =
        try id(Before) of
            before -> {Before, of_clause}
        catch
            _ -> {Before, handler}
        end,
    show(R),
    show({
        try
            throw(Before)
        catch
            Before -> same
        end,
        Before
    }),
    show([
        try
            throw(a)
        catch
            A -> A
        end,
        try
            id(b)
        catch
            _ -> c
        end,
        catch throw(d)
    ]),
    show(
        case
            try
                error(e)
            catch
                error:e -> caught
            end
        of
            caught -> in_case
        end
    ),
    show(
        try
            1 + id(1)
        catch
            _ -> no
        end + 1
    ),
    show(
        try
            begin
                X1 = id(4),
                X1 * 2
            end
        catch
            _ -> no
        end
    );
main(["uncaught_error"]) ->
    show(
        try
            id(1)
        catch
            _ -> no
        end
    ),
    try
        match(id(1), id(2))
    catch
        throw:_ -> no
    end;
main(["uncaught_throw"]) ->
    try
        throw({deep, 1})
    catch
        error:_ -> no
    end;
main(["uncaught_exit"]) ->
    try
        exit(stop)
    catch
        throw:_ -> no;
        error:_ -> no
    end;
main(["try_clause"]) ->
    try id(9) of
        1 -> one
    catch
        _:_ -> handler
    end;
main(["halt"]) ->
    show(
        try
            erlang:halt(4)
        catch
            _:_ -> caught
        end
    ),
    erlang:display(never);
main(Args) ->
    erlang:display(Args).

%% Hide the stack of an error, which Clause leaves empty until stack traces exist.
show({'EXIT', {Reason, Stack}}) when is_list(Stack) -> erlang:display({'EXIT', {Reason, stack}});
show(Value) -> erlang:display(Value).

%% Opaque values keep the OTP compiler from folding the failing operations below.
id(X) -> X.

%% The class variable binds the class atom of each raise.
class_of(Kind) ->
    try raise(Kind) of
        V -> {returned, V}
    catch
        Class:Reason -> {Class, Reason}
    end.

raise(fun_throw) -> throw(t);
raise(fun_error) -> error(e);
raise(fun_exit) -> exit(x);
raise(fun_none) -> none.

%% Runtime failures are caught as error:Reason.
runtime(Kind) ->
    try fail(Kind) of
        V -> {returned, V}
    catch
        error:Reason -> {error, Reason}
    end.

fail(fun_badmatch) -> match(id(1), id(2));
fail(fun_clause) -> clause(id(other));
fail(fun_badarith) -> id(1) + id(a);
fail(fun_case) -> select_one(id(3));
fail(fun_if) -> positive(id(0));
fail(fun_badarg) -> id(1) andalso true;
fail(fun_badkey) -> (id(#{}))#{k := 1};
fail(fun_badmap) -> (id(nomap))#{k := 1};
fail(fun_badrecord) -> (id({other, 1, 2}))#pt.x;
fail(fun_record) -> (id(#pt{x = 1, y = 2}))#pt.x.

match(A, B) ->
    A = B.

clause(one) -> one.

select_one(X) ->
    case X of
        1 -> one
    end.

positive(X) ->
    if
        X > 0 -> positive
    end.

%% Reason patterns of every term kind select the first matching clause in order.
select(Value) ->
    try throw(Value) of
        _ -> unreachable
    catch
        {tag, N} when N > 0 -> {positive_tag, N};
        {tag, N} -> {tag, N};
        {_, N} -> {any_tag, N};
        #{key := K} -> {map, K};
        #pt{x = X, y = Y} -> {record, X, Y};
        <<First, Rest/binary>> -> {binary, First, Rest};
        Other -> {other, Other}
    end.

%% A variable bound before the try is an equality constraint in a catch pattern.
bound(Expected, Thrown) ->
    try
        throw(Thrown)
    catch
        Expected -> {matched, Expected};
        Different -> {different, Different}
    end.

%% A repeated variable requires the class and the reason to be equal.
same_atom(Reason) ->
    try
        throw(Reason)
    catch
        Same:Same -> {same, Same};
        Class:Other -> {differ, Class, Other}
    end.

%% Guards on catch clauses, including a guard that raises, reject the clause and try the next.
guarded(Value) ->
    try
        throw(Value)
    catch
        N when N + 1 > 1 -> {positive, N};
        N when -N > 0 -> {negative, N};
        N -> {neither, N}
    end.

%% of clauses select on the body value; no match raises {try_clause, Value}.
sign(Value) ->
    try id(Value) of
        N when is_integer(N), N > 0 -> positive;
        N when is_integer(N), N < 0 -> negative;
        0 -> zero
    catch
        _ -> thrown
    end.

%% The of clauses see the names bound in the body.
of_scope(Value) ->
    try Doubled = Value * 2 of
        Result -> {Doubled, Result}
    catch
        _ -> no
    end.

%% An exception in an of clause or a handler body is not caught by the same try.
of_raises(Value) ->
    try id(Value) of
        _ -> throw(from_of)
    catch
        from_of -> caught_by_same
    end.

handler_raises(Value) ->
    try
        throw(Value)
    catch
        _ -> throw(from_handler)
    end.

of_value(Value) ->
    try id(Value) of
        2 ->
            {two,
                try id(nested) of
                    nested -> inner_of
                catch
                    _ -> no
                end}
    catch
        _ -> no
    end.
