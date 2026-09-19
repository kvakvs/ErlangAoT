-module(structure).
-define(ID(X), X).
f(L,M,B) -> {
  ?ID([X, X+1, {X} || X <- L, X > 0]),
  #{K => V, V := K || K := V <:- M},
  [X || X <- L && <<Y>> <:= B && K := V <- M, true, A <- L && B0 <- L],
  [X || true && false],
  [X || f() <- L],
  [X || k() := v() <- M],
  [[Y || Y <- X] || X <- L],
  << (f(X)) || X <- L >>,
  << begin X end || X <- L >>,
  << #{K => V || K := V <- M} || true >>,
  #{[X || X <- L] => << <<X>> || X <- L >> || true},
  [X || (case X of _ -> true end)],
  [X || X = 1]
}.
g({[X || X <- L]}) -> ok.
