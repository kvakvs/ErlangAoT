-module(funs_try_maybe).
-define(ID(X), X).
-include("syntax.hrl").
f(M,F,A) -> {
  fun local/0, fun m:f/2, fun M:F/A, fun M:f/0, fun m:F/1,
  ?ID(fun (X) when is_atom(X); X =:= 1 -> X; (_) -> no end),
  ?ID(fun Loop(0) -> done; Loop(N) -> Loop(N-1) end),
  ?ID(try a,b after cleanup(),done end),
  ?ID(try a of f() -> yes; _ -> no after cleanup() end),
  ?ID(try catch f() catch R -> R; error:{a,R}:S when is_list(S) -> {R,S}; C:E -> {C,E} end),
  try a of X -> X catch throw:R -> R after begin a,b end end,
  try a catch R -> try b after c end after done end,
  maybe a, b end,
  ?ID(maybe {ok,X} ?= g(), X = 1, X ?= 1, X else error -> no; f() when true; false -> impossible end),
  maybe case a of a -> yes end ?= ok end,
  ?INCLUDED_FUN, ?INCLUDED_MAYBE, 'maybe', 'else'
}.
