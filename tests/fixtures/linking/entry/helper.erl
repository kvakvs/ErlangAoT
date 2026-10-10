%% Entry candidates: one without arguments, a non-default name, the wrong arity and a private function.
-module(helper).
-export([run/0, start/1, pair/2]).

run() -> ok.

start(_Args) -> ok.

hidden(_Args) -> ok.

pair(_, _) -> ok.
