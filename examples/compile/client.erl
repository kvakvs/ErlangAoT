-module(client).
-export([value/0,demo/0]).
value() -> answer:identity(answer:value()).
demo() -> answer:demo().
