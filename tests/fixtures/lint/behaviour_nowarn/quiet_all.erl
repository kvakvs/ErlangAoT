%% nowarn_behaviours skips the whole check, including the module name errors.
-module(quiet_all).
-compile([nowarn_behaviours]).
-behaviour(shape).
-behaviour(no_such_behaviour).
-behaviour('').
