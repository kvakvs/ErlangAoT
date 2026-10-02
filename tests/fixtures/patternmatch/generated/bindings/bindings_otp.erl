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
-module(bindings_otp).
-export([gh_6516_scope1/0,gh_6516_scope2/0,mutable_variables_1/0,match_right_tuple_1/1,force_succ_regs/2,id/1]).
gh_6516_scope1() ->
    {X = 4, X = 3}.
gh_6516_scope2() ->
  {X = 4, _ = X = 3}.
mutable_variables_1() ->
    Zero = 0,
    One = 1,
    Result = One = Zero,
    {Result,One,Zero}.
match_right_tuple_1(T) ->
    {A, _} = T,
    {_, B} = A,
    %% The call ensures that A is in {x,0} and B is in {x,1}
    id(force_succ_regs(A, B)).
force_succ_regs(_A, B) -> B.
id(I) -> I.
