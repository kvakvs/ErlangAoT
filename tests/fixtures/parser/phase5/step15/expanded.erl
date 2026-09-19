-module(expanded_specs).
-include("../step13/attributes.hrl").
-define(ARG, A :: integer()).
-define(BOUND, A :: integer()).
-spec (run(?ARG) -> #included{} when ?BOUND).
-callback check(A) -> boolean() when is_subtype(A, integer()).
run(A) -> A.
