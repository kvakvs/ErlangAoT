-module(control).
f(X) -> begin case X of a -> ok; _ -> no end end, if X > 0 -> yes; true -> no end, receive M -> M after 0 -> timeout end, receive after 1 -> ok end.
