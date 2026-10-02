-module(answer).
-export([value/0, identity/1, classify/1, demo/0]).
-record(sample,{value}).
-spec value() -> integer().
value() -> 42.
identity(X) -> Y=X, Y.
classify(#sample{value=V}) when is_integer(V) -> record;
classify(#{value := V}) when is_integer(V) -> map;
classify(<<V:8,_/bitstring>>) when is_integer(V) -> binary;
classify([V|_]) when is_integer(V) -> list;
classify(X) when is_integer(X,-100,100) -> integer;
classify(_) -> other.
demo() -> {classify(#sample{value=1}),classify(#{value=>1}),
           classify(<<7,8>>),classify([2]),classify(42),classify(no)}.
