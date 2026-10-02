-module(rec_guard_size_negative).
% Record semantic fixture.
-record(r,{a}). f(X) when is_record(X,r,-1) -> ok.
