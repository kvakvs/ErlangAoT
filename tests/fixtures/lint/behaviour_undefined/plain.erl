%% A module without callbacks has no behaviour_info/1.
-module(plain).
-export([id/1]).

id(X) -> X.
