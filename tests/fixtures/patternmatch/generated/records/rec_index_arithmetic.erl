-module(rec_index_arithmetic).
% Record semantic fixture.
-record(r,{a}). f(#r.a+1) -> ok.
