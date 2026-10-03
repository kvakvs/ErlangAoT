-module(rec_field).
% Record semantic fixture.
-record(r, {a}).
f(X) -> X#r.missing.
