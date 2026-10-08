-module(afters).
-export([main/1]).
-compile(nowarn_deprecated_catch).

%% The first argument selects a scenario; erlang:display/1 traces the order of bodies, clauses and after bodies.
main(["normal"]) ->
    show(
        try
            note(body)
        after
            note(cleanup)
        end
    ),
    show(
        try note(body) of
            body -> note(of_clause)
        after
            note(cleanup)
        end
    ),
    show(
        try
            note(body)
        catch
            _ -> note(handler)
        after
            note(cleanup)
        end
    ),
    show(
        try note(body) of
            body -> note(of_clause)
        catch
            _ -> note(handler)
        after
            note(cleanup)
        end
    ),
    show(
        try
            id(value)
        after
            id(discarded)
        end
    ),
    show({
        try
            id(first)
        after
            note(cleanup)
        end,
        note(next)
    });
main(["caught"]) ->
    show(
        try
            throw(note(ball))
        catch
            Ball -> {caught, Ball}
        after
            note(cleanup)
        end
    ),
    show(
        try error(note(oops)) of
            _ -> no
        catch
            error:E -> note({handler, E})
        after
            note(cleanup)
        end
    ),
    show(
        try
            match(id(1), id(2))
        catch
            error:{badmatch, V} -> {badmatch, V}
        after
            note(cleanup)
        end
    ),
    show(
        try
            try
                throw(inner)
            after
                note(inner_after)
            end
        catch
            inner -> note(outer_handler)
        after
            note(outer_after)
        end
    );
main(["uncaught"]) ->
    show(
        catch (try
            throw(note(passing))
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try
            error(note(passing))
        catch
            throw:_ -> no
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try exit(note(passing)) of
            _ -> no
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try id(9) of
            1 -> one
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try id(1) of
            1 -> throw(note(from_of))
        catch
            from_of -> no
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try
            throw(a)
        catch
            a -> throw(note(from_handler))
        after
            note(cleanup)
        end)
    ),
    show(
        catch (try
            raiser:depth(note(2))
        after
            note(cleanup)
        end)
    );
main(["replaced"]) ->
    show(
        catch (try
            note(body)
        after
            throw(note(from_after))
        end)
    ),
    show(
        catch (try
            throw(original)
        after
            throw(note(from_after))
        end)
    ),
    show(
        catch (try
            error(original)
        catch
            throw:_ -> no
        after
            exit(note(from_after))
        end)
    ),
    show(
        catch (try
            throw(original)
        after
            match(id(1), id(3))
        end)
    ),
    show(
        catch (try
            throw(original)
        after
            (catch throw(swallowed))
        end)
    );
main(["nested"]) ->
    show(
        try
            try
                note(inner_body)
            after
                note(inner_after)
            end
        after
            note(outer_after)
        end
    ),
    show(
        catch (try
            try
                throw(note(deep))
            after
                note(inner_after)
            end
        after
            note(outer_after)
        end)
    ),
    show(
        try
            note(body)
        after
            try
                throw(note(in_after))
            catch
                in_after -> note(handled_in_after)
            after
                note(after_after)
            end
        end
    ),
    Before = id(before),
    show(
        try
            note(Before)
        after
            note({cleanup, Before})
        end
    ),
    show(
        try
            throw(x)
        catch
            x -> Before
        after
            note(Before)
        end
    ),
    show(raiser:wrapped(id(ok))),
    show(catch raiser:wrapped(id(fail)));
main(["uncaught_top"]) ->
    try
        throw(note({top, 1}))
    after
        note(cleanup)
    end;
main(["after_top"]) ->
    try
        note(body)
    after
        error(note(from_after))
    end;
main(["halt"]) ->
    try
        erlang:halt(note(5))
    after
        note(never)
    end;
main(Args) ->
    erlang:display(Args).

%% Hide the stack of an error, which Clause leaves empty until stack traces exist.
show({'EXIT', {Reason, Stack}}) when is_list(Stack) -> erlang:display({'EXIT', {Reason, stack}});
show(Value) -> erlang:display({value, Value}).

%% Display a step and return it, so each body shows when it runs.
note(Step) ->
    erlang:display(Step),
    Step.

%% Opaque values keep the OTP compiler from folding the failing operations below.
id(X) -> X.

match(A, B) ->
    A = B.
