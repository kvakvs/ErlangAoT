-module(client).
-compile({no_auto_import, [is_integer/1]}).
-export([nested_skip/1, nested_reach/1, strict/1, local_shadow/1, qualified/1]).
nested_skip(X) -> true orelse answer:required_zero(X).
nested_reach(X) -> false orelse answer:required_zero(X).
strict(X) -> true or answer:required_zero(X).
local_shadow(X) -> is_integer(X).
qualified(X) -> erlang:is_integer(X).
is_integer(X) -> X.
