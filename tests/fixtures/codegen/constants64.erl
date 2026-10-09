-module(constants).
-export([value/0, negative/0, zero/0, minimum/0, maximum/0]).
-spec value() -> atom() | integer().
value() -> 42.
negative() -> -42.
zero() -> 0.
minimum() -> -576460752303423488.
maximum() -> 576460752303423487.
