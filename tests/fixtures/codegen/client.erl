-module(client).
-export([value/0]).
value() -> answer:identity(answer:value()).
