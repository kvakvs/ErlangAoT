-module(rec_default_capture).
% Record semantic fixture.
-record(r, {a = X}).
f(X) -> #r{}.
