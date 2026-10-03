%% Entry candidates with the wrong arity, a non-default name and a private function.
-module(helper).
-export([run/0, start/1]).

run() -> ok.

start(_Args) -> ok.

hidden(_Args) -> ok.
