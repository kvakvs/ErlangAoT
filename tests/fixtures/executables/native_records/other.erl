-module(other).
-export([point/1]).

%% Another module's native record with the same name as one in natives.
-record(#point{x = 0, y = 0}).

point(X) -> #point{x = X}.
