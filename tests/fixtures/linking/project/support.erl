-module(support).
-export([value/0]).

%% A library module shared by the tool and library targets.
value() -> 7.
