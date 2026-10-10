%% A hand-written behaviour_info/1 cannot sit beside -callback attributes.
-module(handwritten).
-export([behaviour_info/1]).

-callback run() -> ok.
-callback stop(term()) -> ok.

behaviour_info(callbacks) -> [{run, 0}].
