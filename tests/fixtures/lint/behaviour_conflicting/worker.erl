-module(worker).

-callback init(term()) -> ok.
-callback info() -> term().
