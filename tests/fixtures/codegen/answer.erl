-module(answer).
-export([value/0, identity/1]).
value() -> 42.
identity(X) -> X.
private() -> -1.
