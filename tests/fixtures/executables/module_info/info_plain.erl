%% Without -vsn the compiler adds one derived from the module's digest.
-module(info_plain).
-export([id/1]).

id(X) -> X.
