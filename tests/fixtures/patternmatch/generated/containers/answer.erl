%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2004-2026. All Rights Reserved.
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
%% Copyright Ericsson AB 2016-2025. All Rights Reserved.
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
-export([str_alias_1/1,tuple_alias_a/1,tuple_alias_b/1,list_in_tuple_a/1,list_in_tuple_b/1,tuple_in_tuple_a/1,tuple_in_tuple_b/1,multiple_aliases_1a/1,multiple_aliases_1b/1,multiple_aliases_2/1,multiple_aliases_3a/1,multiple_aliases_3b/1,multiple_aliases_4a/1,multiple_aliases_4b/1,list_alias1a/1,list_alias1b/1,list_alias2a/1,list_alias2b/1,list_alias3a/1,list_alias3b/1,first/2,id/1,do_tuple/0,do_literal_tuple_1/1,do_literal_tuple_2/1,make/1,head_tail/1,prefix/1,nested_prefix/1,empty_prefix/1,repeat/2,independent/1,extract/1,body/1,fallthrough/1,badmatch/1,order/2,queries/1,guard/1,guard_construct/1,head/1,tail/1,length_value/1,size_value/1,element_value/2,wrong_spec/1,source_order/1,failure_order/1,wide/1]).
str_alias_1([$a,$b,$c]="abc"="a"++[$b,$c]=[97,98,99]) -> ok;
str_alias_1([$d|"ef"]="def") -> ok;
str_alias_1([$g|"hi"]="g"++"hi"="gh"++"i"="ghi"++"") -> ok;
str_alias_1("k"++"lm"=[$k|"lm"]) -> ok;
str_alias_1([113,114,115]="qrs"=[$q,$r,$s]="q"++"r"++"s") -> ok;
str_alias_1([$x,$y]="xy") -> ok;
str_alias_1(""=[]) -> ok;
str_alias_1(_) -> error.
tuple_alias_a({A,B,C} = {X,Y,Z}) ->
    {A,B,C,X,Y,Z};
tuple_alias_a({A,B} = {C,D} = {E,F}) ->
    {A,B,C,D,E,F}.
tuple_alias_b({_,_,_}=Expr) ->
    {A,B,C} = {X,Y,Z} = Expr,
    {A,B,C,X,Y,Z};
tuple_alias_b({_,_}=Expr) ->
    {A,B} = {C,D} = {E,F} = Expr,
    {A,B,C,D,E,F}.
list_in_tuple_a({container, [_,_,_] = [A,B,C], D}) ->
    {A,B,C,D}.
list_in_tuple_b(E) ->
    {container, [_,_,_] = [A,B,C], D} = E,
    {A,B,C,D}.
tuple_in_tuple_a({x, {y,A} = {B,C}, D} = {E, F, G}) ->
    {A,B,C,D,E,F,G}.
tuple_in_tuple_b(Expr) ->
    {x, {y,A} = {B,C}, D} = {E, F, G} = Expr,
    {A,B,C,D,E,F,G}.
multiple_aliases_1a((A=B) = (C=D)) ->
    {A,B,C,D}.
multiple_aliases_1b(Expr) ->
    (A=B) = (C=D) = Expr,
    {A,B,C,D}.
multiple_aliases_2((A=B) = (A=C)) ->
    {A,B,C}.
multiple_aliases_3a((A={_,_}=B)={_,_}=C) ->
    {A,B,C}.
multiple_aliases_3b(Expr) ->
    (A={_,_}=B) = {_,_} = C = Expr,
    {A,B,C}.
multiple_aliases_4a((A=[_,_,_]=B) = [_,_,_] = C) ->
    {A,B,C}.
multiple_aliases_4b(Expr) ->
    (A=[_,_,_]=B) = [_,_,_] = C = Expr,
    {A,B,C}.
list_alias1a([a,b]=[X,Y]) ->
    {X,Y}.
list_alias1b(Expr) ->
    [a,b] = [X,Y] = Expr,
    {X,Y}.
list_alias2a([X,Y]=[a,b]) ->
    {X,Y}.
list_alias2b(Expr) ->
    [X,Y] = [a,b] = Expr,
    {X,Y}.
list_alias3a([X,b]=[a,Y]) ->
    {X,Y}.
list_alias3b(Expr) ->
    [X,b] = [a,Y]= Expr,
    {X,Y}.
first(Fst, _Snd) -> Fst.
id(I) -> I.
do_tuple() ->
    {0, _} = {necessary}.
do_literal_tuple_1(X) ->
    element(X, {1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1}).
do_literal_tuple_2(X) ->
    element(X, {2,2,2,2,2, 2,2,2,2,2, 2,2,2,2,2, 2,2,2,2,2}).
make(X) -> {id(X), [X,{X},"Ω"|X], "abc"}.
head_tail(_) -> 1 = hd(id([1])), [] = tl(id([1])), ok.
prefix("ab" ++ T) -> T; prefix(_) -> no.
nested_prefix([97|[98|[]]] ++ T) -> T; nested_prefix(_) -> no.
empty_prefix([] ++ T) -> T.
repeat(X,X) -> equal; repeat(_,_) -> different.
independent(X) -> A = {X,[X]}, B = {X,[X]}, A = B, A =:= B.
extract({_,[A|T]}) -> id({A,T}); extract(X) -> X.
body(X) -> {A,[B|T]} = X, {A,B,T}.
fallthrough({A,A}) -> A; fallthrough([A,A]) -> A; fallthrough({_,A}) -> A; fallthrough(X) -> X.
badmatch(X) -> {impossible} = {X,[X]}, unreachable.
order(X,Y) -> {X =:= Y, X == Y, X < Y, X =< Y, X > Y, X >= Y, min(X,Y), max(X,Y)}.
queries(X) -> {is_tuple(X), is_list(X), is_atom(X), is_integer(X)}.
guard(X) when length(X) =:= 2 -> two; guard(X) when tuple_size(X) =:= 2 -> pair; guard(_) -> other.
guard_construct(X) when element(2,{a,X}) =:= hd([X]) -> X.
head(X) -> hd(X).
tail(X) -> tl(X).
length_value(X) -> length(X).
size_value(X) -> size(X).
element_value(N,X) -> element(N,X).
-spec wrong_spec(integer()) -> integer().
wrong_spec({_,X}) -> X; wrong_spec([X|_]) -> X; wrong_spec(X) -> X.
source_order(X) -> T = {A = id(X), B = id(X)}, {T,A,B}.
failure_order(X) -> {hd(X), ok = impossible}.
wide(X) -> {X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X}.
