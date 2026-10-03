-module(rec_guard_size).
% Record semantic fixture.
-record(r, {a}).
f(X) when is_record(X, r, X) -> ok.
