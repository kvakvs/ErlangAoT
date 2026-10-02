-module(legacy_record_shadow).
% Guard catalog semantic case.
-record(r,{a}). -compile({no_auto_import,[is_record/2]}). f(X) when record(X,r) -> X. is_record(X,Y) -> {X,Y}.
