-module(constants).
-export([value/0, negative/0, zero/0, minimum/0, maximum/0]).
value() -> 42.
negative() -> -42.
zero() -> 0.
minimum() -> -134217728.
maximum() -> 134217727.
