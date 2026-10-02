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
-module(answer).
-export([repeat/2,aliases/1,literal/1,atom/1,nil/1,tuple/1,wildcard/2,constant/1,low32/1,high32/1,low64/1,high64/1,wrong_spec/2]).
repeat(X, X) -> X.
aliases((A=B) = (A=C)) -> C.
literal(118=$v) -> ok.
atom(ok = X) -> X.
nil(""=[]) -> [].
tuple({}=X) -> X.
wildcard(_Name, _) -> _Name.
constant(1+2*3) -> 7.
low32(-134217728) -> low.
high32(134217727) -> high.
low64(-576460752303423488) -> low.
high64(576460752303423487) -> high.
-spec wrong_spec(integer(), integer()) -> integer().
wrong_spec(X,X) -> X.
