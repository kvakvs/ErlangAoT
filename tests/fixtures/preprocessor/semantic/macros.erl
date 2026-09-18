-module(macros).
-define(LATE, ?VALUE).
-define(VALUE, 42).
-define(OBJECT, target).
-define(F(X), {X,X}).
-define(F(), zero).
-define(F, object).
-define(OPEN, {).
-define(CLOSE, }).
-define(EMPTY,).
-define(S(X), ??X).
-define(UNUSED(X), ok).
first() -> {?LATE, ?OBJECT(), ?F, ?F(), ?F(?F(7)), ?UNUSED(?MISSING)}, ?OPEN a,b ?CLOSE, [?EMPTY].
strings() -> {?S(16#ff + ?VALUE), ?S('quoted atom'), ?S("line\n"), ?S($\n), ?S(1.25), ?S('if')}.
-undef(VALUE).
-define(VALUE, 99).
last() -> ?LATE.
sigils() -> {?S(~b"λ"), ?S(~B"raw"), ?S(~"str"), ?S(~s"ok"abc)}.
controls() -> {?S($\s), ?S($'), ?S("\0001"), ?S("\001\200")}.
