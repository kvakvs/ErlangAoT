-module(scope).
-export([main/1]).

%% Names bound in every clause are usable after the case; subject bindings are visible everywhere.
exported(Input) ->
    case Value = Input of
        {ok, V} -> Kind = ok;
        {error, V} when is_atom(V) -> Kind = error;
        V -> Kind = raw
    end,
    {Kind, V, Value}.

%% A name exported by a nested case counts as bound by its enclosing clause.
nested(A, B) ->
    case A of
        1 ->
            case B of
                1 -> X = one_one;
                _ -> X = one_other
            end;
        _ ->
            X = other
    end,
    X.

%% An exported name is readable in later guards and bodies (OTP warns when a pattern reuses it).
reuse(A) ->
    case A of
        {K, _} -> ok;
        K -> ok
    end,
    case A of
        {First, {Second, V}} when First =:= K, Second =:= K -> {pair, V};
        _ -> {single, K}
    end.

%% Clause-local names stay local; only the case value leaves the expression.
local(A) ->
    Result =
        case A of
            1 ->
                Temporary = 10,
                Temporary + 1;
            _ ->
                Other = 20,
                Other + A
        end,
    Result.

%% begin/end evaluates a sequence in the enclosing scope and yields its last value.
block(X) ->
    Total = begin
        Double = X * 2,
        Double + 1
    end,
    {Total, Double, begin
        X
    end}.

%% A case as a tuple element, beside a sibling that reads only earlier bindings.
sibling(X) ->
    {
        case X of
            1 -> a;
            _ -> b
        end,
        X
    }.

main(["clause"]) ->
    exported(fine),
    case nested(1, 1) of
        one_other -> unexpected
    end;
main(["badmatch"]) ->
    case reuse(a) of
        {single, K} -> {pair, _} = reuse(K)
    end;
main(_) ->
    erlang:display([
        exported({ok, 1}), exported({error, gone}), exported({error, 7}), exported(plain)
    ]),
    erlang:display([nested(1, 1), nested(1, 2), nested(2, 1)]),
    erlang:display([reuse({k, {k, v}}), reuse({k, other}), reuse(single)]),
    erlang:display([local(1), local(5)]),
    erlang:display([block(3), sibling(1), sibling(2)]).
