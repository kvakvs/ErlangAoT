-module(fact_budget).
-export([identity/1, constant/0, compound/1, extract/1, record/1]).
-record(r,{a}).
-spec identity(integer()) -> integer().
identity(X) -> Y=X, Y.
constant() -> X=42, Y=X, Y.
compound(X) -> T={X,[X],#{a => X},<<3:2>>,#r{a=X},1.5}, T.
extract({X,[Y],#{a := Z},<<V:8>>}) when is_integer(X,-10,10) -> {X,Y,Z,V};
extract(X) -> X.
record(#r{a=X}) -> X;
record(X) -> X.
