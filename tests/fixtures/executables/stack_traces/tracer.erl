-module(tracer).
-export([outer/1]).

%% A remote frame above a local raise.
outer(Kind) -> {outer, inner(Kind)}.

inner(error) -> error(remote);
inner(Kind) -> Kind.
