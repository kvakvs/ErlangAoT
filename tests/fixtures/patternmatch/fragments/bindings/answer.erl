-module(answer).
-export([force_succ_regs/2, identity/1]).
-spec force_succ_regs(integer(), atom()) -> atom().
force_succ_regs(Discarded, Selected) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
identity(X) -> client:id(X).
