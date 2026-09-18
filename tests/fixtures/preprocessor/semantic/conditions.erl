-module(conditions).
-define(X, true).
-if(defined(X) andalso not defined(NO)).
first() -> yes.
-else.
first() -> no.
-endif.
-ifdef(NO).
-include("missing.hrl").
invalid() -> ?MISSING.
-define(BAD(whatever), ignored).
-else.
-if((1 bsl 100) + 1 > (1 bsl 100) andalso 1 == 1.0 andalso 1 =/= 1.0).
big() -> yes.
-endif.
-endif.
-if(1 div 0).
arithmetic() -> wrong.
-elif(42).
arithmetic() -> wrong.
-else.
arithmetic() -> false_condition.
-endif.
-if(map_get(key, #{key => 7}) == 7 andalso element(2,{a,b}) == b andalso length([1,2|[]]) == 2).
containers() -> yes.
-endif.
-if(bit_size(<<1:3,2:5>>) == 8 andalso byte_size(<<1:3>>) == 1 andalso is_binary(<<1,2>>)).
bits() -> yes.
-endif.
-if(true orelse 1 div 0).
short_circuit() -> yes.
-endif.
