-module(rec_wild_name).
% Record semantic fixture.
-record(r, {a, b}).
f() -> #r{X = 1}.
