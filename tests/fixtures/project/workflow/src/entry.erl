-module(entry).
-include("choice.hrl").
-include_lib("demo/include/common.hrl").
-define(PRIVATE, true).
-ifdef(FAIL).
-error(injected_app_failure).
-endif.
value() -> {?VALUE, ?PICK, ?LIB}.
