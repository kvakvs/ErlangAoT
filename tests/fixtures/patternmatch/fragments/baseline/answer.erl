-module(answer).
-export([first/2]).
-export([identity/1]).
first(Selected, Discarded) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
identity(X) -> client:id(X).
