-module(legacy_record_old).
% Guard catalog semantic case.
-record(r,{a}). f(X) when record(X,r) -> X. record(X,Y) -> {X,Y}.
