-module(helper).
-export([double/1]).

%% A remote value consumed by a conditional match.
double(N) when is_integer(N) -> {ok, N * 2};
double(_) -> error.
