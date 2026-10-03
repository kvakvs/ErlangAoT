-module(rec_guard_tag).
% Record semantic fixture.
-record(r, {a}).
f(X) when is_record(X, X) -> ok.
