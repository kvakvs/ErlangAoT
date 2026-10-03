-module(rec_guard_update).
% Record semantic fixture.
-record(r, {a}).
f(X) when X#r{a = 1} == X -> ok.
