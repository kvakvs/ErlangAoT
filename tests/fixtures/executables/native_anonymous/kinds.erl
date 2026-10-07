-module(kinds).
-export([open/1, closed/1]).
-export_record([open]).

%% An exported and a private record of another module.
-record(#open{value = 0, label = open}).
-record(#closed{value = 0}).

open(V) -> #open{value = V}.
closed(V) -> #closed{value = V}.
