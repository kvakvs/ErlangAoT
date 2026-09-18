-feature(compr_assign, enable).
-module(features).
-if(?FEATURE_AVAILABLE(maybe_expr) andalso ?FEATURE_ENABLED(compr_assign)).
-define(AVAILABLE, yes).
-else.
-define(AVAILABLE, no).
-endif.
-feature(maybe_expr, disable).
features() -> {?FEATURE_AVAILABLE(maybe_expr), ?FEATURE_AVAILABLE(missing), ?FEATURE_ENABLED(maybe_expr), ?AVAILABLE}.
words() -> {maybe, else}.
