-module(server).

-callback init(term()) -> ok.
-callback stop() -> ok.
-callback info() -> term().
-optional_callbacks([info/0]).
