-module(rec_wild_unused).
% Record semantic fixture.
-record(r,{a,b}). f(#r{a=1,b=2,_=X}) -> X.
