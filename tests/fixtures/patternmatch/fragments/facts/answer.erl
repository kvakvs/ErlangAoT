-module(answer).
-export([
    identity_plain/1,
    identity_spec/1,
    constant_plain/0,
    constant_spec/0,
    alias_plain/1,
    alias_spec/1,
    projected_plain/2,
    projected_spec/2,
    extracted_plain/1,
    extracted_spec/1,
    joined_plain/1,
    joined_spec/1,
    map_plain/2,
    map_spec/2,
    bits_plain/1,
    bits_spec/1,
    record_plain/1,
    record_spec/1,
    allocation_plain/1,
    allocation_spec/1,
    candidate_plain/1,
    candidate_spec/1,
    guards_plain/1,
    guards_spec/1,
    force_succ_regs_plain/2,
    force_succ_regs_spec/2,
    id/1
]).
-record(r, {a, b}).
identity_plain(X) ->
    Y = X,
    id(Y).
-spec identity_spec(integer()) -> integer().
identity_spec(X) ->
    Y = X,
    id(Y).
constant_plain() ->
    Y = 42,
    Z = Y,
    id(Z).
-spec constant_spec() -> integer().
constant_spec() ->
    Y = 42,
    Z = Y,
    id(Z).
alias_plain(X) ->
    (A = B) = X,
    {A, B}.
-spec alias_spec(integer()) -> integer() | tuple().
alias_spec(X) ->
    (A = B) = X,
    {A, B}.
projected_plain(X, Y) ->
    A = X,
    B = Y,
    id(A),
    B.
-spec projected_spec(integer(), integer()) -> integer().
projected_spec(X, Y) ->
    A = X,
    B = Y,
    id(A),
    B.
extracted_plain({X, {Y, Z}}) when is_integer(X), is_integer(Y, 0, 10) -> {X, Y, Z};
extracted_plain([X | Y]) -> {X, Y};
extracted_plain(X) -> {fallback, X}.
-spec extracted_spec(integer()) -> integer() | tuple().
extracted_spec({X, {Y, Z}}) when is_integer(X), is_integer(Y, 0, 10) -> {X, Y, Z};
extracted_spec([X | Y]) -> {X, Y};
extracted_spec(X) -> {fallback, X}.
joined_plain({_, Z}) -> Z;
joined_plain([X | _]) -> X;
joined_plain(X) -> X.
-spec joined_spec(integer()) -> integer().
joined_spec({_, Z}) -> Z;
joined_spec([X | _]) -> X;
joined_spec(X) -> X.
map_plain(K, M) ->
    #{K := V} = M,
    [V, map_get(K, M)].
-spec map_spec(integer(), integer()) -> integer() | list().
map_spec(K, M) ->
    #{K := V} = M,
    [V, map_get(K, M)].
bits_plain(<<N:8, V:N, T/bitstring>>) -> {N, V, T};
bits_plain(_) -> no.
-spec bits_spec(integer()) -> integer() | tuple() | atom().
bits_spec(<<N:8, V:N, T/bitstring>>) -> {N, V, T};
bits_spec(_) -> no.
record_plain(#r{a = X, b = Y}) when is_tuple(X) ->
    {Z} = X,
    Y = Z,
    Y;
record_plain(_) ->
    no.
-spec record_spec(integer()) -> integer().
record_spec(#r{a = X, b = Y}) when is_tuple(X) ->
    {Z} = X,
    Y = Z,
    Y;
record_spec(_) ->
    no.
allocation_plain(X) ->
    A = {X, [X], #{key => X}, <<3:2>>},
    {_, [Y], #{key := Z}, <<V:2>>} = A,
    {Y, Z, V}.
-spec allocation_spec(integer()) -> integer() | tuple().
allocation_spec(X) ->
    A = {X, [X], #{key => X}, <<3:2>>},
    {_, [Y], #{key := Z}, <<V:2>>} = A,
    {Y, Z, V}.
candidate_plain({X, Y}) when is_tuple(X), element(1, X) == Y -> X;
candidate_plain({X, Y}) when is_list(X), hd(X) == Y -> Y;
candidate_plain(V) -> V.
-spec candidate_spec(integer()) -> integer().
candidate_spec({X, Y}) when is_tuple(X), element(1, X) == Y -> X;
candidate_spec({X, Y}) when is_list(X), hd(X) == Y -> Y;
candidate_spec(V) -> V.
guards_plain(X) when
    element(1, X) == true;
    map_get(ok, X) == true;
    is_integer(X, -1267650600228229401496703205376, 1267650600228229401496703205376)
->
    Y = X,
    Y;
guards_plain(X) ->
    {fallback, X}.
-spec guards_spec(integer()) -> integer() | tuple().
guards_spec(X) when
    element(1, X) == true;
    map_get(ok, X) == true;
    is_integer(X, -1267650600228229401496703205376, 1267650600228229401496703205376)
->
    Y = X,
    Y;
guards_spec(X) ->
    {fallback, X}.
force_succ_regs_plain(Discarded, Selected) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
-spec force_succ_regs_spec(integer(), integer()) -> integer().
force_succ_regs_spec(Discarded, Selected) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
id(Value) ->
    Saved = Value,
    Saved.
