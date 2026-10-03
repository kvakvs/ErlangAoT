-module(rec_duplicate).
% Record semantic fixture.
-record(r, {a}).
f() -> #r{a = 1, a = 2}.
