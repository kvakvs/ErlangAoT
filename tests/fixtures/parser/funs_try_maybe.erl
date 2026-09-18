-module(funs_try_maybe).
f(X) -> fun f/1, fun M:F/A, fun Loop(0) -> ok; Loop(N) -> Loop(N-1) end, try X of Y -> Y catch error:R:S -> {R,S}; T -> T after ok end, maybe {ok,V} ?= X, V else _ -> no end.
