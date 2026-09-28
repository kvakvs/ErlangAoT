-define(B, b).
-define(
 'NAME'(A, B),
 {A, ?B, "-endif.").
f() -> ?NAME(a,B)}.