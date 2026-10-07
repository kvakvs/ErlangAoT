-module(clospeer).
-export([make/1, call/2, scopes/1]).

-record(point, {x, y}).

% Closures created here are called from another module.
make(Base) ->
    Point = #point{x = Base, y = Base + 1},
    fun
        (x) -> Point#point.x;
        (y) -> Point#point.y;
        ({add, N}) -> make(Base + N)
    end.

call(F, Arg) -> F(Arg).

% Captured names bound by every case clause, inside try, and read by fun heads as map keys.
scopes(Input) ->
    case Input of
        {a, V} -> Tag = first;
        V -> Tag = other
    end,
    Tagged = fun(X) -> {Tag, V, X} end,
    Safe =
        try Input + 1 of
            Sum -> fun() -> {sum, Sum} end
        catch
            error:Reason -> fun() -> {caught, Reason} end
        end,
    Key = lookup,
    Lookup = fun
        (#{Key := Found}) -> {found, Found};
        (_) -> missing
    end,
    [Tagged(1), Safe(), Lookup(#{lookup => 7}), Lookup(#{other => 7})].
