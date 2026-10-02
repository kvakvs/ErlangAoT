-module(rec_pattern_update).
% Record semantic fixture.
-record(r,{a}). f(X) -> X#r{a=1} = X.
