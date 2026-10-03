#!/usr/bin/env escript
-module(boom).

%% Escript entries exit 127 on uncaught exceptions.
main(_Args) -> 1 = 2.
