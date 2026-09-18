-module(lint_only).
f(X) -> case X of g() -> ok end.
g() -> ok.
