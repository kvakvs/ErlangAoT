-module(funpeer).
-export([double/1, apply_twice/2, hidden_caller/0, pick/1]).

double(X) -> X * 2.

apply_twice(F, X) -> F(F(X)).

% A local fun of a function this module does not export.
hidden_caller() -> fun hidden/1.

hidden(X) -> {hidden, X}.

pick(add) -> fun funvals:add/2;
pick(double) -> fun double/1;
pick(missing) -> fun funvals:missing/1;
pick(private) -> fun funvals:private/1;
pick(elsewhere) -> fun nowhere:thing/0.
