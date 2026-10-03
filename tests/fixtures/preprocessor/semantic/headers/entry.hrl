-ifndef(LOCAL_ENTRY_HRL).
-define(LOCAL_ENTRY_HRL, true).
-record(entry, {
    bytes = 0 :: non_neg_integer(),
    kind = regular :: regular | directory,
    modified = undefined :: integer() | undefined,
    payload :: term()
}).
-endif.
