-module(rescan).
-define(A, ?B).
-define(B, x).
-define(B(X), X).
?A(3).
-define(C(), ?B).
?C()(4).
-define(FIELD, field).
?FILE(,).
?LINE(,).
