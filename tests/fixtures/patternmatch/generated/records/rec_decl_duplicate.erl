-module(rec_decl_duplicate).
% Record semantic fixture.
-record(r,{a}). -record(r,{b}). f() -> ok.
