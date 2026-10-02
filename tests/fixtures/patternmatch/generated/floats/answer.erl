%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2002-2026. All Rights Reserved.
%%
%% Licensed under the Apache License, Version 2.0 (the "License");
%% you may not use this file except in compliance with the License.
%% You may obtain a copy of the License at
%%
%%     http://www.apache.org/licenses/LICENSE-2.0
%%
%% Unless required by applicable law or agreed to in writing, software
%% distributed under the License is distributed on an "AS IS" BASIS,
%% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%% See the License for the specific language governing permissions and
%% limitations under the License.
%%
%% %CopyrightEnd%
%%

%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2015-2026. All Rights Reserved.
%%
%% Licensed under the Apache License, Version 2.0 (the "License");
%% you may not use this file except in compliance with the License.
%% You may obtain a copy of the License at
%%
%%     http://www.apache.org/licenses/LICENSE-2.0
%%
%% Unless required by applicable law or agreed to in writing, software
%% distributed under the License is distributed on an "AS IS" BASIS,
%% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%% See the License for the specific language governing permissions and
%% limitations under the License.
%%
%% %CopyrightEnd%
%%
-module(answer).
-export([pc/3,add/2,guard_add/2,subtract/2,guard_subtract/2,multiply/2,guard_multiply/2,divide/2,guard_divide/2,value_float/1,guard_float/1,value_round/1,guard_round/1,value_trunc/1,guard_trunc/1,value_floor/1,guard_floor/1,value_ceil/1,guard_ceil/1,value_abs/1,guard_abs/1,compare/2,nested/2,repeat/2,literal/1,body/1,constant/0,positive/1,negative/1,classify/1,integer_only/1,legacy/1,float_compare/1,wrong_spec/2,ordered/2,id/1]).
pc(Cov, NotCov, X) ->
    round(Cov/(Cov+NotCov)*100) + 42 + 2.0*X.
add(X,Y) -> X + Y.
guard_add(X,Y) when is_number(X + Y) -> yes; guard_add(_,_) -> no.
subtract(X,Y) -> X - Y.
guard_subtract(X,Y) when is_number(X - Y) -> yes; guard_subtract(_,_) -> no.
multiply(X,Y) -> X * Y.
guard_multiply(X,Y) when is_number(X * Y) -> yes; guard_multiply(_,_) -> no.
divide(X,Y) -> X / Y.
guard_divide(X,Y) when is_number(X / Y) -> yes; guard_divide(_,_) -> no.
value_float(X) -> erlang:float(X).
guard_float(X) when is_number(erlang:float(X)) -> yes; guard_float(_) -> no.
value_round(X) -> erlang:round(X).
guard_round(X) when is_number(erlang:round(X)) -> yes; guard_round(_) -> no.
value_trunc(X) -> erlang:trunc(X).
guard_trunc(X) when is_number(erlang:trunc(X)) -> yes; guard_trunc(_) -> no.
value_floor(X) -> erlang:floor(X).
guard_floor(X) when is_number(erlang:floor(X)) -> yes; guard_floor(_) -> no.
value_ceil(X) -> erlang:ceil(X).
guard_ceil(X) when is_number(erlang:ceil(X)) -> yes; guard_ceil(_) -> no.
value_abs(X) -> erlang:abs(X).
guard_abs(X) when is_number(erlang:abs(X)) -> yes; guard_abs(_) -> no.
compare(X,Y) -> {X =:= Y,X =/= Y,X == Y,X /= Y,X < Y,X =< Y,X > Y,X >= Y,min(X,Y),max(X,Y)}.
nested(X,Y) -> {{X,[X]} =:= {Y,[Y]},{X,[X]} == {Y,[Y]},{X,[X]} < {Y,[Y]}}.
repeat(X,X) -> same; repeat(_,_) -> different.
literal(1.0) -> one; literal(-0.0) -> negative_zero; literal(0.0) -> positive_zero; literal(_) -> other.
body(X) -> 1.0 = X, X.
constant() -> {1.0,-0.0,0.0,1.7976931348623157e308,4.9406564584124654e-324}.
positive(X) -> +X.
negative(X) -> -X.
classify(X) -> {is_number(X),is_float(X),is_integer(X)}.
integer_only(X) -> X div 1.
legacy(X) when float(X) -> yes; legacy(_) -> no.
float_compare(X) when X > 0 -> X + 1.0 > 0; float_compare(X) -> X + 1.0, false.
-spec wrong_spec(integer(),integer()) -> integer().
wrong_spec(X,Y) -> X + Y.
ordered(X,Y) -> {X / Y, ok = impossible}.
id(X) -> X.
