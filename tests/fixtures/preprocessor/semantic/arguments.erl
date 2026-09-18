-module(arguments).
-define(ID(X), X).
-define(PAIR(X,Y), {X,Y}).
-type callback() :: ?ID(fun((atom(), integer()) -> ok)).
f() -> ?PAIR(case yes of yes -> a,b end, fun(X) when X > 0, is_integer(X) -> X; (_) -> 0 end),
       ?ID(fun erlang:length/1),
       ?ID(fun Named(X) -> Named(X) end),
       ?PAIR(#{a => {1,2}, b => <<1,2>>}, [a,b]),
       ?PAIR(try a,b catch _:_ -> c,d after e,f end, receive X -> X after 0 -> ok end),
       ?PAIR(maybe X ?= ok, X else _ -> error end, begin a,b end).
