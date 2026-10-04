#!/usr/bin/env escript
-module(boom).

%% Escript entries exit 127 on uncaught exceptions of every class.
main(["throw"]) -> throw(oops);
main(_Args) -> 1 = 2.
