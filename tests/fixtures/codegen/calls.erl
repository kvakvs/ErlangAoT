-module(calls).
-export([value/0, equal/0, forward/1]).
value() -> project(left(), identity(right()), left()).
equal() -> project(7, 7, 7).
forward(X) -> identity(X).
project(_, Y, _) -> Y.
identity(X) -> X.
left() -> 11.
right() -> -7.
