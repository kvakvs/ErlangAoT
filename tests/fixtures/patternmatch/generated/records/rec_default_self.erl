-module(rec_default_self).
% Record semantic fixture.
-record(r,{a=#r{}}). f() -> #r{}.
