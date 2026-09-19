-module(alternatives).
-define(ID(X), X).
f(M,F,A) -> {
  ?ID(fun M:F/A), fun m:f/A, fun M:f/A, fun m:F/A, fun M:F/1,
  fun () -> ok end,
  try a of X -> X catch R -> R end,
  maybe a else _ -> no end,
  maybe a ?= b end
}.
