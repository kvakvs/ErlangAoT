-module(contextual).
-spec spec() -> spec.
-spec callback() -> callback.
-spec record() -> record.
-'callback' 'spec'(A) -> A when A :: term().
spec() -> spec.
callback() -> callback.
record() -> record.
-record plain, ({a, (b) = fun() -> ok end}).
-record #Other({field}).
