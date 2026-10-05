-module(maybes).
-export([main/1]).

%% The first argument selects a scenario; each maybe result is displayed.
main(["success"]) ->
    show(chain(id({ok, 1}), id({ok, 2}))),
    show(last_match(id({ok, 3}))),
    show(single(id(7))),
    show(
        maybe
            {ok, A} ?= id({ok, 4}),
            B = A * 10,
            {ok, C} ?= id({ok, B + 1}),
            {A, B, C}
        end
    );
main(["exit"]) ->
    show(chain(id({error, first}), id({ok, 2}))),
    show(chain(id({ok, 1}), id({error, second}))),
    show(chain(id(other), id({ok, 2}))),
    show(last_match(id(nope))),
    show(traced(id(stop)));
main(["else"]) ->
    show(classify(id({ok, 5}))),
    show(classify(id({error, enoent}))),
    show(classify(id({error, 42}))),
    show(classify(id(timeout))),
    show(classify(id(<<"bin">>))),
    show(outside(id(9), id({error, x}))),
    show(outside(id(9), id({ok, x})));
main(["nested"]) ->
    show(nested(id({ok, {ok, 1}}))),
    show(nested(id({ok, {error, inner}}))),
    show(nested(id({error, outer}))),
    show(in_case(id(left))),
    show(in_case(id(right)));
main(["errors"]) ->
    show(caught(id(unknown))),
    show(caught(id({error, 1}))),
    show(raising(id(ok))),
    show(
        try
            raising(id(boom))
        catch
            error:R -> {caught, R}
        end
    ),
    show(
        try
            strict(id({ok, 2}))
        catch
            error:B -> {caught, B}
        end
    ),
    show(remote(id({ok, 3}))),
    show(remote(id(missing)));
main(["else_clause"]) ->
    show(classify(id(unexpected))).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% Two conditional matches; the first mismatch is the result.
chain(First, Second) ->
    maybe
        {ok, X} ?= First,
        {ok, Y} ?= Second,
        X + Y
    end.

%% A conditional match as the last expression yields its matched value.
last_match(Value) ->
    maybe
        {ok, _} ?= Value
    end.

single(Value) ->
    maybe
        Value
    end.

%% Expressions after a mismatch are not evaluated.
traced(Value) ->
    maybe
        show(before),
        go ?= Value,
        show(skipped),
        done
    end.

%% Else clauses select on the unmatched value, with guards.
classify(Value) ->
    maybe
        {ok, N} ?= Value,
        {found, N}
    else
        {error, Reason} when is_atom(Reason) -> {failed, Reason};
        {error, Code} when is_integer(Code), Code > 10 -> {code, Code};
        timeout -> retry;
        Other when is_binary(Other) -> {binary, Other}
    end.

%% Else clauses see the bindings made before the maybe.
outside(Before, Value) ->
    maybe
        {ok, Got} ?= Value,
        {Before, Got}
    else
        {error, Why} -> {Before, Why}
    end.

nested(Value) ->
    maybe
        {ok, Inner} ?= Value,
        Result =
            maybe
                {ok, N} ?= Inner,
                {inner, N}
            else
                {error, Why} -> {inner_else, Why}
            end,
        {outer, Result}
    else
        {error, Outer} -> {outer_else, Outer}
    end.

in_case(Side) ->
    case Side of
        left ->
            maybe
                1 ?= id(1),
                {left, ok}
            end;
        right ->
            maybe
                1 ?= id(2),
                {right, ok}
            else
                2 -> {right, two}
            end
    end.

%% A missing else clause raises {else_clause, Value}.
caught(Value) ->
    try
        classify(Value)
    catch
        error:{else_clause, V} -> {else_clause, V}
    end.

%% Exceptions inside a maybe body propagate.
raising(Value) ->
    maybe
        ok ?= Value,
        fine
    else
        boom -> error(from_else)
    end.

%% An ordinary match inside a maybe still raises badmatch.
strict(Value) ->
    maybe
        {ok, N} ?= Value,
        {ok, 3} = id({ok, N}),
        matched
    end.

remote(Value) ->
    maybe
        {ok, N} ?= Value,
        {ok, M} ?= helper:double(N),
        M
    else
        Other -> {other, Other}
    end.
