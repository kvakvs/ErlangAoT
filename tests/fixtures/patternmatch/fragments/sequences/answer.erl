-module(answer).
-export([
    mutable_variables_1/0,
    id/1,
    bind/1,
    check_only/1,
    self/1,
    rebind/2,
    chain/1,
    chain_check/2,
    compound/1,
    compound_check/2,
    rhs_binding/1,
    rhs_old/1,
    wild/1,
    literal/1,
    constant/1,
    empty_list/1,
    empty_tuple/1,
    stop/1,
    body_failure/1,
    later_call/1,
    in_call/1,
    sibling/2,
    first/2,
    chained_failure/0,
    early_call_error/1,
    wrong_spec/1,
    wide/1,
    deep/1
]).
mutable_variables_1() ->
    Earlier = -2,
    Later = 5,
    Selected = Later = Earlier,
    Selected.
id(Value) ->
    Saved = Value,
    Saved.
bind(X) ->
    Y = X,
    Y.
check_only(X) -> X = 1.
self(X) -> X = X.
rebind(X, Y) ->
    X = Y,
    X.
chain(X) ->
    A = B = C = X,
    A = B,
    C.
chain_check(X, Y) ->
    Z = X = Y,
    Z.
compound(X) ->
    (A = B) = X,
    A = B,
    B.
compound_check(X, Y) ->
    (A = X) = Y,
    A.
rhs_binding(X) ->
    Y = (Y = X),
    Y.
rhs_old(X) ->
    X = (Y = X),
    Y.
wild(X) ->
    _ = X,
    _ = ok,
    _Name = X,
    _Name.
literal(X) ->
    pattern_atom = X,
    ok.
constant(X) ->
    (1 + 2) = X,
    X.
empty_list(X) ->
    "" = X,
    [].
empty_tuple(X) ->
    {} = X,
    X.
stop(X) ->
    ok = X,
    hd([]).
body_failure(X) when is_integer(X) ->
    ok = X,
    later;
body_failure(_) ->
    fallback.
later_call(X) ->
    first,
    Y = id(X),
    id(Y).
in_call(X) ->
    id(A = B = X),
    A = B,
    A.
sibling(X, Y) ->
    first(A = X, A = Y),
    A.
first(Selected, Discarded) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
chained_failure() ->
    A = 1 = 2 = 3,
    A.
early_call_error(X) ->
    hd(X),
    ok = impossible.
-spec wrong_spec(integer()) -> integer().
wrong_spec(X) ->
    Y = X,
    Y.
wide(X) ->
    V0 = X,
    V1 = X,
    V2 = X,
    V3 = X,
    V4 = X,
    V5 = X,
    V6 = X,
    V7 = X,
    V8 = X,
    V9 = X,
    V10 = X,
    V11 = X,
    V12 = X,
    V13 = X,
    V14 = X,
    V15 = X,
    V16 = X,
    V17 = X,
    V18 = X,
    V19 = X,
    V20 = X,
    V21 = X,
    V22 = X,
    V23 = X,
    V24 = X,
    V25 = X,
    V26 = X,
    V27 = X,
    V28 = X,
    V29 = X,
    V30 = X,
    V31 = X,
    V32 = X,
    V33 = X,
    V34 = X,
    V35 = X,
    V36 = X,
    V37 = X,
    V38 = X,
    V39 = X,
    V40 = X,
    V41 = X,
    V42 = X,
    V43 = X,
    V44 = X,
    V45 = X,
    V46 = X,
    V47 = X,
    V48 = X,
    V49 = X,
    V50 = X,
    V51 = X,
    V52 = X,
    V53 = X,
    V54 = X,
    V55 = X,
    V56 = X,
    V57 = X,
    V58 = X,
    V59 = X,
    V60 = X,
    V61 = X,
    V62 = X,
    V63 = X,
    V64 = X,
    V65 = X,
    V66 = X,
    V67 = X,
    V68 = X,
    V69 = X,
    V70 = X,
    V71 = X,
    V72 = X,
    V73 = X,
    V74 = X,
    V75 = X,
    V76 = X,
    V77 = X,
    V78 = X,
    V79 = X,
    V80 = X,
    V81 = X,
    V82 = X,
    V83 = X,
    V84 = X,
    V85 = X,
    V86 = X,
    V87 = X,
    V88 = X,
    V89 = X,
    V90 = X,
    V91 = X,
    V92 = X,
    V93 = X,
    V94 = X,
    V95 = X,
    V96 = X,
    V97 = X,
    V98 = X,
    V99 = X,
    V100 = X,
    V101 = X,
    V102 = X,
    V103 = X,
    V104 = X,
    V105 = X,
    V106 = X,
    V107 = X,
    V108 = X,
    V109 = X,
    V110 = X,
    V111 = X,
    V112 = X,
    V113 = X,
    V114 = X,
    V115 = X,
    V116 = X,
    V117 = X,
    V118 = X,
    V119 = X,
    V120 = X,
    V121 = X,
    V122 = X,
    V123 = X,
    V124 = X,
    V125 = X,
    V126 = X,
    V127 = X,
    V127.
deep(X) ->
    V0 =
        V1 =
        V2 =
        V3 =
        V4 =
        V5 =
        V6 =
        V7 =
        V8 =
        V9 =
        V10 =
        V11 =
        V12 =
        V13 =
        V14 =
        V15 =
        V16 =
        V17 =
        V18 =
        V19 =
        V20 =
        V21 =
        V22 =
        V23 =
        V24 =
        V25 =
        V26 =
        V27 =
        V28 =
        V29 =
        V30 =
        V31 =
        V32 =
        V33 =
        V34 =
        V35 =
        V36 =
        V37 =
        V38 =
        V39 =
        V40 =
        V41 =
        V42 =
        V43 =
        V44 =
        V45 =
        V46 =
        V47 =
        V48 =
        V49 =
        V50 =
        V51 =
        V52 =
        V53 =
        V54 =
        V55 =
        V56 =
        V57 =
        V58 =
        V59 =
        V60 =
        V61 =
        V62 =
        V63 =
        V64 =
        V65 =
        V66 =
        V67 =
        V68 =
        V69 =
        V70 =
        V71 =
        V72 =
        V73 =
        V74 =
        V75 =
        V76 =
        V77 =
        V78 =
        V79 =
        V80 =
        V81 =
        V82 =
        V83 =
        V84 =
        V85 =
        V86 =
        V87 =
        V88 =
        V89 =
        V90 =
        V91 =
        V92 =
        V93 =
        V94 =
        V95 =
        V96 =
        V97 =
        V98 =
        V99 =
        V100 =
        V101 =
        V102 =
        V103 =
        V104 =
        V105 =
        V106 =
        V107 =
        V108 =
        V109 =
        V110 =
        V111 =
        V112 =
        V113 =
        V114 =
        V115 =
        V116 = V117 = V118 = V119 = V120 = V121 = V122 = V123 = V124 = V125 = V126 = V127 = X,
    V0.
