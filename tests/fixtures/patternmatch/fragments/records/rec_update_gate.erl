-module(rec_update_gate).
% Record semantic fixture.
-record(r, {a}).
f(X) -> X#r{a = 1}.
