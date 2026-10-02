-module(legacy_record_suppress).
% Guard catalog semantic case.
-record(r,{a}). -compile({no_auto_import,[record/2]}). f(X) when record(X,r) -> X.
