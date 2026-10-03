-module(client).
-export([same/0, truth/0, falsity/0, ok_value/0, unicode/0, empty/0, nul/0, projected/0]).
same() -> true.
truth() -> answer:id(answer:truth()).
falsity() -> answer:id(answer:falsity()).
ok_value() -> answer:id(answer:ok_value()).
unicode() -> answer:id(answer:unicode()).
empty() -> answer:id(answer:empty()).
nul() -> answer:id(answer:nul()).
projected() -> answer:id(answer:projected()).
