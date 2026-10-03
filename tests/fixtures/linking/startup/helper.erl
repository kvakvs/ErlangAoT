-module(helper).
-export([value/0, pick/1]).

value() -> {error, 42}.

pick(some) -> ok.
