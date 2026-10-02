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
-export([semicolon/2,comma/2,mixed/3,alias/2,head_first/1,term_join/2,nested/2,nested_semi/2,bad_or/1,bad_semi/1,bad_rhs_or/1,bad_rhs_semi/1,bad_comma/1,skip_hd_or/1,skip_hd_and/1,reach_hd_and/1,strict_hd_or/1,strict_hd_and/1,strict_hd_xor/1,strict_semi/1,not_bad/1,not_semi/1,skip_bad_not/1,skip_bad_not_or/1,body_bad_or/1,body_strict_or/1,body_strict_and/1,not_and/2,not_or/2,qualified_and/2,qualified_not/1,required_zero/1,local_skip/1,local_reach/1,wrong_spec/2,csemi4_orelse_a/4,csemi4_orelse_b/4,csemi4_orelse_c/4,csemi4_orelse_d/4,lazy_and/2,guard_lazy_and/2,lazy_or/2,guard_lazy_or/2,strict_and/2,guard_strict_and/2,strict_or/2,guard_strict_or/2,strict_xor/2,guard_strict_xor/2,not_value/1,guard_not/1,wide_semi/1,wide_comma/1,deep_lazy/1]).
semicolon(A,B) when A; B -> ok.
comma(A,B) when A, B -> ok.
mixed(A,B,C) when A, B; C -> ok.
alias(X=Y,X) when Y =:= false, hd([]); Y =:= X -> Y.
head_first(0) when hd([]); true -> ok.
term_join(X,Y) when (X andalso Y) =:= 7 -> ok.
nested(X,Y) when ((X andalso Y) orelse true) -> ok.
nested_semi(X,Y) when ((X andalso Y) orelse true); true -> ok.
bad_or(_) when hd([]) orelse true -> ok.
bad_semi(_) when hd([]); true -> ok.
bad_rhs_or(_) when (false orelse hd([])) orelse true -> ok.
bad_rhs_semi(_) when (false orelse hd([])) orelse true; true -> ok.
bad_comma(_) when true, (hd([]) orelse true); true, true -> ok.
skip_hd_or(_) when true orelse hd([]) -> ok.
skip_hd_and(_) when false andalso hd([]) -> ok.
reach_hd_and(_) when true andalso hd([]) -> ok.
strict_hd_or(_) when true or hd([]) -> ok.
strict_hd_and(_) when false and hd([]) -> ok.
strict_hd_xor(_) when true xor hd([]) -> ok.
strict_semi(_) when true or hd([]); true -> ok.
not_bad(_) when not glurf -> ok.
not_semi(_) when not glurf; true -> ok.
skip_bad_not(_) -> false andalso not glurf.
skip_bad_not_or(_) -> true orelse not glurf.
body_bad_or(_) -> hd([]) orelse true.
body_strict_or(_) -> true or hd([]).
body_strict_and(_) -> false and hd([]).
not_and(X,Y) -> not X andalso not Y.
not_or(X,Y) -> not X orelse not Y.
qualified_and(X,Y) -> erlang:'and'(X,Y).
qualified_not(X) -> erlang:'not'(X).
required_zero(0) -> true.
local_skip(X) -> false andalso required_zero(X).
local_reach(X) -> true andalso required_zero(X).
-spec wrong_spec(integer(), integer()) -> integer().
wrong_spec(X,Y) -> X andalso Y.
csemi4_orelse_a(A, X, B, Y) when (tuple_size(A) > 1) orelse (X > 1);
			 (tuple_size(B) > 1) orelse (Y > 1) -> ok.
csemi4_orelse_b(A, X, B, Y) when (X > 1) orelse (tuple_size(A) > 1);
			 (tuple_size(B) > 1) orelse (Y > 1) -> ok.
csemi4_orelse_c(A, X, B, Y) when (tuple_size(A) > 1) orelse (X > 1);
                           (Y > 1) orelse (tuple_size(B) > 1) -> ok.
csemi4_orelse_d(A, X, B, Y) when (X > 1) or (tuple_size(A) > 1);
			 (Y > 1) or (tuple_size(B) > 1) -> ok.
lazy_and(X,Y) -> X andalso Y.
guard_lazy_and(X,Y) when X andalso Y -> ok.
lazy_or(X,Y) -> X orelse Y.
guard_lazy_or(X,Y) when X orelse Y -> ok.
strict_and(X,Y) -> X and Y.
guard_strict_and(X,Y) when X and Y -> ok.
strict_or(X,Y) -> X or Y.
guard_strict_or(X,Y) when X or Y -> ok.
strict_xor(X,Y) -> X xor Y.
guard_strict_xor(X,Y) when X xor Y -> ok.
not_value(X) -> not X.
guard_not(X) when not X -> ok.
wide_semi(X) when X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; X; true -> ok.
wide_comma(X) when true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, X; true -> ok.
deep_lazy(X) -> ((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((X andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true) andalso true).
