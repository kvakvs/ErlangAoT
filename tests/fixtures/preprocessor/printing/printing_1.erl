-module(values).
-define(TEXT(X), ??X).
-define(VALUE, 16#ffff_ffff_ffff_ffff_ffff).
f(R, X) -> {?VALUE, ?LINE, ?FILE, ?MODULE, ?TEXT(a + 1),
  1.0, 1.2345678901234567e-200, 1.0e308, $\s, $\n, $\x{1f600},
  'fun', 'can\'t', 'Î»', "a\\b\n\"\000Î»", "one" "two",
  R#rec.field, #rec.field, R#_.field, #{key => X}, <<X:8/integer>>, + +1, - -1}.
