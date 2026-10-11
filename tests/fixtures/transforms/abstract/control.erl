-module(control).
-export([branches/1, exceptions/1, waits/1, maybes/1, comprehensions/2]).

branches(X) ->
    Y =
        case X of
            {ok, V} when is_integer(V), V > 0; V =:= zero ->
                V;
            [_ | _] = L ->
                length(L);
            _ ->
                none
        end,
    Z =
        if
            X > 1, X < 10 -> small;
            X >= 10 -> big;
            true -> other
        end,
    begin
        {Y, Z}
    end.

exceptions(F) ->
    try F() of
        {ok, V} -> V;
        Other -> Other
    catch
        {thrown, R} -> R;
        error:badarg -> badarg;
        exit:Reason:Stack when is_list(Stack) -> {Reason, Stack};
        Class:Any:_ -> {Class, Any}
    after
        cleanup
    end,
    try
        F()
    after
        done
    end,
    try
        F()
    catch
        throw:{a, B} -> B
    end.

waits(T) ->
    receive
        {msg, M} when M > 0 -> M;
        stop -> stop
    after T ->
        timeout
    end,
    receive
        any -> ok
    end,
    receive
    after 0 -> nothing
    end.

maybes(X) ->
    maybe
        {ok, A} ?= X,
        B = A + 1,
        {ok, C} ?= {ok, B},
        C
    end,
    maybe
        {ok, D} ?= X,
        D
    else
        {error, E} -> E;
        _ -> unknown
    end.

comprehensions(L, B) ->
    L1 = [X * 2 || X <- L, X > 0],
    L2 = [{X, Y} || X <- L, Y <- L, X =/= Y],
    L3 = [X || <<X:8>> <= B],
    L4 = <<<<X:8>> || X <- L>>,
    L5 = #{K => V || K := V <- #{a => 1}},
    L6 = [{P, Q} || P <- L && Q <- L],
    L7 = [{P, Q} || P <- L && Q <- L, P > 0],
    L8 = [X || X <:- L],
    L9 = [X || <<X>> <:= B],
    L10 = #{K => V || K := V <:- #{b => 2}},
    L11 = [X, X + 1 || X <- L],
    {L1, L2, L3, L4, L5, L6, L7, L8, L9, L10, L11}.
