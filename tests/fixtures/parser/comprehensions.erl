-module(comprehensions).
f(L,M,B) -> [X || X <:- L], [X,Y || X <- L && Y <- L], #{K=>V || K:=V <:- M}, << <<X>> || <<X>> <:= B >>, [Y || X <- L, Y = X+1].
