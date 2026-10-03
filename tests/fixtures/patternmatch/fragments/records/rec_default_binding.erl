-module(rec_default_binding).
% Record semantic fixture.
-record(r, {a = (X = 1)}).
f() -> #r{}.
