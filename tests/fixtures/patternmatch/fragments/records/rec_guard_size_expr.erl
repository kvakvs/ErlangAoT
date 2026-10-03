-module(rec_guard_size_expr).
% Record semantic fixture.
-record(r, {a}).
f(X) when is_record(X, r, 1 + 2) -> ok.
