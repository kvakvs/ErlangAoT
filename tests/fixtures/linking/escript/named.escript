#!/usr/local/bin/escript
-module(named).

%% Explicit module, no export: main/1 is exported implicitly, helper/0 is not.
main(_Args) -> helper().

helper() -> ok.
