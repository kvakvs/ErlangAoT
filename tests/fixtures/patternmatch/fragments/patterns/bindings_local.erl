-module(bindings_local).
-export([
    gh_6516_scope1/0,
    gh_6516_scope2/0,
    mutable_variables_1/0,
    match_right_tuple_1/1,
    force_succ_regs/2,
    id/1
]).
gh_6516_scope1() -> {Slot = 8, Slot = -2}.
gh_6516_scope2() -> {Slot = 8, _ = Slot = -2}.
mutable_variables_1() ->
    Earlier = -2,
    Later = 5,
    Selected = Later = Earlier,
    {Selected, Earlier, Later}.
match_right_tuple_1(Input) ->
    {Outer, _Ignored} = Input,
    {_Tag, Selected} = Outer,
    Result = force_succ_regs(Outer, Selected),
    id(Result).
force_succ_regs(Discarded, Selected) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
id(Value) ->
    Saved = Value,
    Saved.
