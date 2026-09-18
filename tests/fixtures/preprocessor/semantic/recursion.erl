-module(recursion).
-define(DROP(X), ok).
-define(A, ?DROP(?A)).
?A.
-undef(A).
-define(A, ?DROP(?B)).
-define(B, ?A).
?A.
-undef(B).
?A.
-define(BAD, ?DROP(,)).
?BAD.
-define(ID(X),X).
?ID(?ID(?ID(3))).
