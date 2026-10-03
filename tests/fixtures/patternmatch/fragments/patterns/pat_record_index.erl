-module(pat_record_index).
-export([f/1]).
-record(r, {x}).
f(#r.x) -> ok.
