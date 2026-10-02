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
-export([id/1,truth/0,falsity/0,ok_value/0,unicode/0,empty/0,nul/0,projected/0]).
-spec truth() -> integer().
truth() -> true.
falsity() -> false.
ok_value() -> ok.
unicode() -> 'λ😀'.
empty() -> ''.
nul() -> 'a\x{0}b'.
id(X) -> X.
projected() -> id((true)).
