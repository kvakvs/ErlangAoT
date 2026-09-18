-module(patterns).
f({X,Y}=A) when X > 0, Y < 2; X =:= 0 -> A; f(_) -> ok.
