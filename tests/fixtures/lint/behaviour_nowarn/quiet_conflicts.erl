-module(quiet_conflicts).
-compile([nowarn_conflicting_behaviours]).
-behaviour(shape).
-behaviour(shape).
-export([name/0]).

name() -> quiet.
