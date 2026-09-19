-module(helper).
-ifdef(PRIVATE).
-error(leaked_macro).
-endif.
value() -> ?VALUE.
