-module(disabled).
-feature(maybe_expr, disable).
-define(ID(X), X).
maybe() -> ?ID({maybe, else}).
