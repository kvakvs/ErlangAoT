%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2001-2026. All Rights Reserved.
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
-export([body_is_atom/1,body_is_integer/1,body_is_number/1,body_is_boolean/1,body_is_tuple/1,body_is_list/1,body_is_binary/1,body_is_bitstring/1,body_is_float/1,body_is_map/1,body_is_pid/1,body_is_port/1,body_is_reference/1,body_is_function/1,guard_is_atom/1,guard_is_integer/1,guard_is_number/1,guard_is_boolean/1,guard_is_tuple/1,guard_is_list/1,guard_is_binary/1,guard_is_bitstring/1,guard_is_float/1,guard_is_map/1,guard_is_pid/1,guard_is_port/1,guard_is_reference/1,guard_is_function/1,exact/2,exact_ne/2,equal/2,unequal/2,less/2,leq/2,greater/2,geq/2,bool/1,legacy_integer/1,legacy_float/1,function_arity/2,guard_arity/2,tuple_size_value/1,length_value/1,size_value/1,head_value/1,tail_value/1,element_value/2,guard_element/2,min_value/2,max_value/2,wrong_spec/1,head_first/1,qualified/1]).
body_is_atom(X) -> is_atom(X).
body_is_integer(X) -> is_integer(X).
body_is_number(X) -> is_number(X).
body_is_boolean(X) -> is_boolean(X).
body_is_tuple(X) -> is_tuple(X).
body_is_list(X) -> is_list(X).
body_is_binary(X) -> is_binary(X).
body_is_bitstring(X) -> is_bitstring(X).
body_is_float(X) -> is_float(X).
body_is_map(X) -> is_map(X).
body_is_pid(X) -> is_pid(X).
body_is_port(X) -> is_port(X).
body_is_reference(X) -> is_reference(X).
body_is_function(X) -> is_function(X).
guard_is_atom(X) when is_atom(X) -> ok.
guard_is_integer(X) when is_integer(X) -> ok.
guard_is_number(X) when is_number(X) -> ok.
guard_is_boolean(X) when is_boolean(X) -> ok.
guard_is_tuple(X) when is_tuple(X) -> ok.
guard_is_list(X) when is_list(X) -> ok.
guard_is_binary(X) when is_binary(X) -> ok.
guard_is_bitstring(X) when is_bitstring(X) -> ok.
guard_is_float(X) when is_float(X) -> ok.
guard_is_map(X) when is_map(X) -> ok.
guard_is_pid(X) when is_pid(X) -> ok.
guard_is_port(X) when is_port(X) -> ok.
guard_is_reference(X) when is_reference(X) -> ok.
guard_is_function(X) when is_function(X) -> ok.
exact(X,Y) -> X =:= Y.
exact_ne(X,Y) -> X =/= Y.
equal(X,Y) -> X == Y.
unequal(X,Y) -> X /= Y.
less(X,Y) -> X < Y.
leq(X,Y) -> X =< Y.
greater(X,Y) -> X > Y.
geq(X,Y) -> X >= Y.
bool(X) when is_boolean(X) -> ok.
legacy_integer(X) when integer(X) -> X.
legacy_float(X) when float(X) -> X.
function_arity(X,N) -> is_function(X,N).
guard_arity(X,N) when is_function(X,N) -> ok.
tuple_size_value(X) -> tuple_size(X).
length_value(X) -> length(X).
size_value(X) -> size(X).
head_value(X) -> hd(X).
tail_value(X) -> tl(X).
element_value(X,Y) -> element(X,Y).
guard_element(X,Y) when element(X,Y) -> ok.
min_value(X,Y) -> min(X,Y).
max_value(X,Y) -> max(X,Y).
-spec wrong_spec(integer()) -> boolean().
wrong_spec(X) -> is_integer(X).
head_first(ok) when element(1,{}) -> impossible.
qualified(X) when erlang:is_atom(X) -> X.
