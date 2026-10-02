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
-export([catalog_0_body/1,catalog_0_qualified/1,catalog_0_guard/1,catalog_1_body/2,catalog_1_qualified/2,catalog_1_guard/2,catalog_2_body/3,catalog_2_qualified/3,catalog_2_guard/3,catalog_3_body/1,catalog_3_qualified/1,catalog_3_guard/1,catalog_4_body/1,catalog_4_qualified/1,catalog_4_guard/1,catalog_5_body/1,catalog_5_qualified/1,catalog_5_guard/1,catalog_6_body/2,catalog_6_qualified/2,catalog_6_guard/2,catalog_7_body/1,catalog_7_qualified/1,catalog_7_guard/1,catalog_8_body/1,catalog_8_qualified/1,catalog_8_guard/1,catalog_9_body/1,catalog_9_qualified/1,catalog_9_guard/1,catalog_10_body/3,catalog_10_qualified/3,catalog_10_guard/3,catalog_11_body/2,catalog_11_qualified/2,catalog_11_guard/2,catalog_12_body/1,catalog_12_qualified/1,catalog_12_guard/1,catalog_13_body/1,catalog_13_qualified/1,catalog_13_guard/1,catalog_14_body/2,catalog_14_qualified/2,catalog_14_guard/2,catalog_15_body/2,catalog_15_qualified/2,catalog_15_guard/2,catalog_16_body/2,catalog_16_qualified/2,catalog_16_guard/2,catalog_19_body/1,catalog_19_qualified/1,catalog_19_guard/1,catalog_21_body/1,catalog_21_qualified/1,catalog_21_guard/1,catalog_22_body/1,catalog_22_qualified/1,catalog_22_guard/1,catalog_23_body/1,catalog_23_qualified/1,catalog_23_guard/1,catalog_24_body/1,catalog_24_qualified/1,catalog_24_guard/1,catalog_25_body/1,catalog_25_qualified/1,catalog_25_guard/1,catalog_26_body/1,catalog_26_qualified/1,catalog_26_guard/1,catalog_27_body/1,catalog_27_qualified/1,catalog_27_guard/1,catalog_28_body/1,catalog_28_qualified/1,catalog_28_guard/1,catalog_29_body/1,catalog_29_qualified/1,catalog_29_guard/1,catalog_30_body/1,catalog_30_qualified/1,catalog_30_guard/1,catalog_31_body/2,catalog_31_qualified/2,catalog_31_guard/2,catalog_32_body/1,catalog_32_qualified/1,catalog_32_guard/1,catalog_33_body/1,catalog_33_qualified/1,catalog_33_guard/1,catalog_34_body/1,catalog_34_qualified/1,catalog_34_guard/1,catalog_35_body/1,catalog_35_qualified/1,catalog_35_guard/1,catalog_36_body/1,catalog_36_qualified/1,catalog_36_guard/1,catalog_37_body/1,catalog_37_qualified/1,catalog_37_guard/1,catalog_38_body/1,catalog_38_qualified/1,catalog_38_guard/1,catalog_39_body/1,catalog_39_qualified/1,catalog_39_guard/1,catalog_40_body/1,catalog_40_qualified/1,catalog_40_guard/1,catalog_42_body/1,catalog_42_qualified/1,catalog_42_guard/1,catalog_43_body/1,catalog_43_qualified/1,catalog_43_guard/1,catalog_44_body/1,catalog_44_qualified/1,catalog_44_guard/1,catalog_45_body/1,catalog_45_qualified/1,catalog_45_guard/1,catalog_46_body/1,catalog_46_qualified/1,catalog_46_guard/1,catalog_47_body/1,catalog_47_qualified/1,catalog_47_guard/1,catalog_48_body/1,catalog_48_qualified/1,catalog_48_guard/1,catalog_49_body/1,catalog_49_qualified/1,catalog_49_guard/1,catalog_50_body/1,catalog_50_qualified/1,catalog_50_guard/1,catalog_51_body/1,catalog_51_qualified/1,catalog_51_guard/1,catalog_52_body/1,catalog_52_qualified/1,catalog_52_guard/1,catalog_53_body/1,catalog_53_qualified/1,catalog_53_guard/1,catalog_54_body/1,catalog_54_qualified/1,catalog_54_guard/1,catalog_55_body/1,catalog_55_qualified/1,catalog_55_guard/1,catalog_56_body/1,catalog_56_qualified/1,catalog_56_guard/1,catalog_57_body/2,catalog_57_qualified/2,catalog_57_guard/2,catalog_58_body/2,catalog_58_qualified/2,catalog_58_guard/2,catalog_59_body/2,catalog_59_qualified/2,catalog_59_guard/2,catalog_60_body/2,catalog_60_qualified/2,catalog_60_guard/2,catalog_61_body/1,catalog_61_qualified/1,catalog_61_guard/1,catalog_62_body/2,catalog_62_qualified/2,catalog_62_guard/2,catalog_63_body/2,catalog_63_qualified/2,catalog_63_guard/2,catalog_64_body/2,catalog_64_qualified/2,catalog_64_guard/2,catalog_65_body/2,catalog_65_qualified/2,catalog_65_guard/2,catalog_66_body/2,catalog_66_qualified/2,catalog_66_guard/2,catalog_67_body/2,catalog_67_qualified/2,catalog_67_guard/2,catalog_68_body/2,catalog_68_qualified/2,catalog_68_guard/2,catalog_69_body/1,catalog_69_qualified/1,catalog_69_guard/1,catalog_70_body/2,catalog_70_qualified/2,catalog_70_guard/2,catalog_71_body/2,catalog_71_qualified/2,catalog_71_guard/2,catalog_72_body/2,catalog_72_qualified/2,catalog_72_guard/2,catalog_73_body/2,catalog_73_qualified/2,catalog_73_guard/2,catalog_74_body/2,catalog_74_qualified/2,catalog_74_guard/2,catalog_75_body/2,catalog_75_qualified/2,catalog_75_guard/2,catalog_76_body/2,catalog_76_qualified/2,catalog_76_guard/2,catalog_77_body/2,catalog_77_qualified/2,catalog_77_guard/2,catalog_78_body/2,catalog_78_qualified/2,catalog_78_guard/2,catalog_79_body/2,catalog_79_qualified/2,catalog_79_guard/2,catalog_80_body/2,catalog_80_qualified/2,catalog_80_guard/2,is_integer_3_guard_1/3,is_integer_3_guard_2/1,is_integer_3_guard_3/1,is_integer_3_guard_4/3,is_integer_3_guard_5/3,is_integer_3_guard_6/3,is_integer_3_guard_7/3,is_integer_3_guard_8/3,is_integer_3_guard_4_id/1,is_integer_3_guard_5_id/1,is_integer_3_guard_8_id/1,bool_min_false/2,bool_min_true/2,bool_max_false/2,max_number/1,min_increment/1,int_clamped_add/1,num_clamped_add/1,map_guard_empty/0,map_guard_empty_2/0,map_get_head/1,map_get_head_not/1,map_is_key_head/1,map_is_key_head_not/1,map_get_head_badmap1/0,map_get_head_badmap2/0,map_get_head_badmap3/0,map_field_check_sequence/1,id/1,conversion_trunc/1,conversion_round/1,conversion_floor/1,conversion_ceil/1,construct/1,update/1,range_alt/3]).
-record(r,{a}).
catalog_0_body(X) -> abs(X).
catalog_0_qualified(X) -> erlang:'abs'(X).
catalog_0_guard(X) when erlang:'abs'(X) -> ok; catalog_0_guard(X) -> no.
catalog_1_body(X,Y) -> binary_part(X,Y).
catalog_1_qualified(X,Y) -> erlang:'binary_part'(X,Y).
catalog_1_guard(X,Y) when erlang:'binary_part'(X,Y) -> ok; catalog_1_guard(X,Y) -> no.
catalog_2_body(X,Y,Z) -> binary_part(X,Y,Z).
catalog_2_qualified(X,Y,Z) -> erlang:'binary_part'(X,Y,Z).
catalog_2_guard(X,Y,Z) when erlang:'binary_part'(X,Y,Z) -> ok; catalog_2_guard(X,Y,Z) -> no.
catalog_3_body(X) -> bit_size(X).
catalog_3_qualified(X) -> erlang:'bit_size'(X).
catalog_3_guard(X) when erlang:'bit_size'(X) -> ok; catalog_3_guard(X) -> no.
catalog_4_body(X) -> byte_size(X).
catalog_4_qualified(X) -> erlang:'byte_size'(X).
catalog_4_guard(X) when erlang:'byte_size'(X) -> ok; catalog_4_guard(X) -> no.
catalog_5_body(X) -> ceil(X).
catalog_5_qualified(X) -> erlang:'ceil'(X).
catalog_5_guard(X) when erlang:'ceil'(X) -> ok; catalog_5_guard(X) -> no.
catalog_6_body(X,Y) -> element(X,Y).
catalog_6_qualified(X,Y) -> erlang:'element'(X,Y).
catalog_6_guard(X,Y) when erlang:'element'(X,Y) -> ok; catalog_6_guard(X,Y) -> no.
catalog_7_body(X) -> float(X).
catalog_7_qualified(X) -> erlang:'float'(X).
catalog_7_guard(X) when erlang:'float'(X) -> ok; catalog_7_guard(X) -> no.
catalog_8_body(X) -> floor(X).
catalog_8_qualified(X) -> erlang:'floor'(X).
catalog_8_guard(X) when erlang:'floor'(X) -> ok; catalog_8_guard(X) -> no.
catalog_9_body(X) -> hd(X).
catalog_9_qualified(X) -> erlang:'hd'(X).
catalog_9_guard(X) when erlang:'hd'(X) -> ok; catalog_9_guard(X) -> no.
catalog_10_body(X,Y,Z) -> is_integer(X,Y,Z).
catalog_10_qualified(X,Y,Z) -> erlang:'is_integer'(X,Y,Z).
catalog_10_guard(X,Y,Z) when erlang:'is_integer'(X,Y,Z) -> ok; catalog_10_guard(X,Y,Z) -> no.
catalog_11_body(X,Y) -> is_map_key(X,Y).
catalog_11_qualified(X,Y) -> erlang:'is_map_key'(X,Y).
catalog_11_guard(X,Y) when erlang:'is_map_key'(X,Y) -> ok; catalog_11_guard(X,Y) -> no.
catalog_12_body(X) -> length(X).
catalog_12_qualified(X) -> erlang:'length'(X).
catalog_12_guard(X) when erlang:'length'(X) -> ok; catalog_12_guard(X) -> no.
catalog_13_body(X) -> map_size(X).
catalog_13_qualified(X) -> erlang:'map_size'(X).
catalog_13_guard(X) when erlang:'map_size'(X) -> ok; catalog_13_guard(X) -> no.
catalog_14_body(X,Y) -> map_get(X,Y).
catalog_14_qualified(X,Y) -> erlang:'map_get'(X,Y).
catalog_14_guard(X,Y) when erlang:'map_get'(X,Y) -> ok; catalog_14_guard(X,Y) -> no.
catalog_15_body(X,Y) -> max(X,Y).
catalog_15_qualified(X,Y) -> erlang:'max'(X,Y).
catalog_15_guard(X,Y) when erlang:'max'(X,Y) -> ok; catalog_15_guard(X,Y) -> no.
catalog_16_body(X,Y) -> min(X,Y).
catalog_16_qualified(X,Y) -> erlang:'min'(X,Y).
catalog_16_guard(X,Y) when erlang:'min'(X,Y) -> ok; catalog_16_guard(X,Y) -> no.
catalog_19_body(X) -> round(X).
catalog_19_qualified(X) -> erlang:'round'(X).
catalog_19_guard(X) when erlang:'round'(X) -> ok; catalog_19_guard(X) -> no.
catalog_21_body(X) -> size(X).
catalog_21_qualified(X) -> erlang:'size'(X).
catalog_21_guard(X) when erlang:'size'(X) -> ok; catalog_21_guard(X) -> no.
catalog_22_body(X) -> tl(X).
catalog_22_qualified(X) -> erlang:'tl'(X).
catalog_22_guard(X) when erlang:'tl'(X) -> ok; catalog_22_guard(X) -> no.
catalog_23_body(X) -> trunc(X).
catalog_23_qualified(X) -> erlang:'trunc'(X).
catalog_23_guard(X) when erlang:'trunc'(X) -> ok; catalog_23_guard(X) -> no.
catalog_24_body(X) -> tuple_size(X).
catalog_24_qualified(X) -> erlang:'tuple_size'(X).
catalog_24_guard(X) when erlang:'tuple_size'(X) -> ok; catalog_24_guard(X) -> no.
catalog_25_body(X) -> is_atom(X).
catalog_25_qualified(X) -> erlang:'is_atom'(X).
catalog_25_guard(X) when erlang:'is_atom'(X) -> ok; catalog_25_guard(X) -> no.
catalog_26_body(X) -> is_binary(X).
catalog_26_qualified(X) -> erlang:'is_binary'(X).
catalog_26_guard(X) when erlang:'is_binary'(X) -> ok; catalog_26_guard(X) -> no.
catalog_27_body(X) -> is_bitstring(X).
catalog_27_qualified(X) -> erlang:'is_bitstring'(X).
catalog_27_guard(X) when erlang:'is_bitstring'(X) -> ok; catalog_27_guard(X) -> no.
catalog_28_body(X) -> is_boolean(X).
catalog_28_qualified(X) -> erlang:'is_boolean'(X).
catalog_28_guard(X) when erlang:'is_boolean'(X) -> ok; catalog_28_guard(X) -> no.
catalog_29_body(X) -> is_float(X).
catalog_29_qualified(X) -> erlang:'is_float'(X).
catalog_29_guard(X) when erlang:'is_float'(X) -> ok; catalog_29_guard(X) -> no.
catalog_30_body(X) -> is_function(X).
catalog_30_qualified(X) -> erlang:'is_function'(X).
catalog_30_guard(X) when erlang:'is_function'(X) -> ok; catalog_30_guard(X) -> no.
catalog_31_body(X,Y) -> is_function(X,Y).
catalog_31_qualified(X,Y) -> erlang:'is_function'(X,Y).
catalog_31_guard(X,Y) when erlang:'is_function'(X,Y) -> ok; catalog_31_guard(X,Y) -> no.
catalog_32_body(X) -> is_integer(X).
catalog_32_qualified(X) -> erlang:'is_integer'(X).
catalog_32_guard(X) when erlang:'is_integer'(X) -> ok; catalog_32_guard(X) -> no.
catalog_33_body(X) -> is_list(X).
catalog_33_qualified(X) -> erlang:'is_list'(X).
catalog_33_guard(X) when erlang:'is_list'(X) -> ok; catalog_33_guard(X) -> no.
catalog_34_body(X) -> is_map(X).
catalog_34_qualified(X) -> erlang:'is_map'(X).
catalog_34_guard(X) when erlang:'is_map'(X) -> ok; catalog_34_guard(X) -> no.
catalog_35_body(X) -> is_number(X).
catalog_35_qualified(X) -> erlang:'is_number'(X).
catalog_35_guard(X) when erlang:'is_number'(X) -> ok; catalog_35_guard(X) -> no.
catalog_36_body(X) -> is_pid(X).
catalog_36_qualified(X) -> erlang:'is_pid'(X).
catalog_36_guard(X) when erlang:'is_pid'(X) -> ok; catalog_36_guard(X) -> no.
catalog_37_body(X) -> is_port(X).
catalog_37_qualified(X) -> erlang:'is_port'(X).
catalog_37_guard(X) when erlang:'is_port'(X) -> ok; catalog_37_guard(X) -> no.
catalog_38_body(X) -> is_record(X,r).
catalog_38_qualified(X) -> erlang:'is_record'(X,r).
catalog_38_guard(X) when erlang:'is_record'(X,r) -> ok; catalog_38_guard(X) -> no.
catalog_39_body(X) -> is_record(X,r,2).
catalog_39_qualified(X) -> erlang:'is_record'(X,r,2).
catalog_39_guard(X) when erlang:'is_record'(X,r,2) -> ok; catalog_39_guard(X) -> no.
catalog_40_body(X) -> is_reference(X).
catalog_40_qualified(X) -> erlang:'is_reference'(X).
catalog_40_guard(X) when erlang:'is_reference'(X) -> ok; catalog_40_guard(X) -> no.
catalog_42_body(X) -> is_tuple(X).
catalog_42_qualified(X) -> erlang:'is_tuple'(X).
catalog_42_guard(X) when erlang:'is_tuple'(X) -> ok; catalog_42_guard(X) -> no.
catalog_43_body(X) -> erlang:'is_integer'(X).
catalog_43_qualified(X) -> erlang:'is_integer'(X).
catalog_43_guard(X) when integer(X) -> ok; catalog_43_guard(X) -> no.
catalog_44_body(X) -> erlang:'is_float'(X).
catalog_44_qualified(X) -> erlang:'is_float'(X).
catalog_44_guard(X) when float(X) -> ok; catalog_44_guard(X) -> no.
catalog_45_body(X) -> erlang:'is_number'(X).
catalog_45_qualified(X) -> erlang:'is_number'(X).
catalog_45_guard(X) when number(X) -> ok; catalog_45_guard(X) -> no.
catalog_46_body(X) -> erlang:'is_atom'(X).
catalog_46_qualified(X) -> erlang:'is_atom'(X).
catalog_46_guard(X) when atom(X) -> ok; catalog_46_guard(X) -> no.
catalog_47_body(X) -> erlang:'is_list'(X).
catalog_47_qualified(X) -> erlang:'is_list'(X).
catalog_47_guard(X) when list(X) -> ok; catalog_47_guard(X) -> no.
catalog_48_body(X) -> erlang:'is_tuple'(X).
catalog_48_qualified(X) -> erlang:'is_tuple'(X).
catalog_48_guard(X) when tuple(X) -> ok; catalog_48_guard(X) -> no.
catalog_49_body(X) -> erlang:'is_pid'(X).
catalog_49_qualified(X) -> erlang:'is_pid'(X).
catalog_49_guard(X) when pid(X) -> ok; catalog_49_guard(X) -> no.
catalog_50_body(X) -> erlang:'is_reference'(X).
catalog_50_qualified(X) -> erlang:'is_reference'(X).
catalog_50_guard(X) when reference(X) -> ok; catalog_50_guard(X) -> no.
catalog_51_body(X) -> erlang:'is_port'(X).
catalog_51_qualified(X) -> erlang:'is_port'(X).
catalog_51_guard(X) when port(X) -> ok; catalog_51_guard(X) -> no.
catalog_52_body(X) -> erlang:'is_binary'(X).
catalog_52_qualified(X) -> erlang:'is_binary'(X).
catalog_52_guard(X) when binary(X) -> ok; catalog_52_guard(X) -> no.
catalog_53_body(X) -> erlang:'is_record'(X,r).
catalog_53_qualified(X) -> erlang:'is_record'(X,r).
catalog_53_guard(X) when record(X,r) -> ok; catalog_53_guard(X) -> no.
catalog_54_body(X) -> erlang:'is_function'(X).
catalog_54_qualified(X) -> erlang:'is_function'(X).
catalog_54_guard(X) when function(X) -> ok; catalog_54_guard(X) -> no.
catalog_55_body(X) -> erlang:'+'(X).
catalog_55_qualified(X) -> erlang:'+'(X).
catalog_55_guard(X) when erlang:'+'(X) -> ok; catalog_55_guard(X) -> no.
catalog_56_body(X) -> erlang:'-'(X).
catalog_56_qualified(X) -> erlang:'-'(X).
catalog_56_guard(X) when erlang:'-'(X) -> ok; catalog_56_guard(X) -> no.
catalog_57_body(X,Y) -> erlang:'*'(X,Y).
catalog_57_qualified(X,Y) -> erlang:'*'(X,Y).
catalog_57_guard(X,Y) when erlang:'*'(X,Y) -> ok; catalog_57_guard(X,Y) -> no.
catalog_58_body(X,Y) -> erlang:'/'(X,Y).
catalog_58_qualified(X,Y) -> erlang:'/'(X,Y).
catalog_58_guard(X,Y) when erlang:'/'(X,Y) -> ok; catalog_58_guard(X,Y) -> no.
catalog_59_body(X,Y) -> erlang:'+'(X,Y).
catalog_59_qualified(X,Y) -> erlang:'+'(X,Y).
catalog_59_guard(X,Y) when erlang:'+'(X,Y) -> ok; catalog_59_guard(X,Y) -> no.
catalog_60_body(X,Y) -> erlang:'-'(X,Y).
catalog_60_qualified(X,Y) -> erlang:'-'(X,Y).
catalog_60_guard(X,Y) when erlang:'-'(X,Y) -> ok; catalog_60_guard(X,Y) -> no.
catalog_61_body(X) -> erlang:'bnot'(X).
catalog_61_qualified(X) -> erlang:'bnot'(X).
catalog_61_guard(X) when erlang:'bnot'(X) -> ok; catalog_61_guard(X) -> no.
catalog_62_body(X,Y) -> erlang:'div'(X,Y).
catalog_62_qualified(X,Y) -> erlang:'div'(X,Y).
catalog_62_guard(X,Y) when erlang:'div'(X,Y) -> ok; catalog_62_guard(X,Y) -> no.
catalog_63_body(X,Y) -> erlang:'rem'(X,Y).
catalog_63_qualified(X,Y) -> erlang:'rem'(X,Y).
catalog_63_guard(X,Y) when erlang:'rem'(X,Y) -> ok; catalog_63_guard(X,Y) -> no.
catalog_64_body(X,Y) -> erlang:'band'(X,Y).
catalog_64_qualified(X,Y) -> erlang:'band'(X,Y).
catalog_64_guard(X,Y) when erlang:'band'(X,Y) -> ok; catalog_64_guard(X,Y) -> no.
catalog_65_body(X,Y) -> erlang:'bor'(X,Y).
catalog_65_qualified(X,Y) -> erlang:'bor'(X,Y).
catalog_65_guard(X,Y) when erlang:'bor'(X,Y) -> ok; catalog_65_guard(X,Y) -> no.
catalog_66_body(X,Y) -> erlang:'bxor'(X,Y).
catalog_66_qualified(X,Y) -> erlang:'bxor'(X,Y).
catalog_66_guard(X,Y) when erlang:'bxor'(X,Y) -> ok; catalog_66_guard(X,Y) -> no.
catalog_67_body(X,Y) -> erlang:'bsl'(X,Y).
catalog_67_qualified(X,Y) -> erlang:'bsl'(X,Y).
catalog_67_guard(X,Y) when erlang:'bsl'(X,Y) -> ok; catalog_67_guard(X,Y) -> no.
catalog_68_body(X,Y) -> erlang:'bsr'(X,Y).
catalog_68_qualified(X,Y) -> erlang:'bsr'(X,Y).
catalog_68_guard(X,Y) when erlang:'bsr'(X,Y) -> ok; catalog_68_guard(X,Y) -> no.
catalog_69_body(X) -> erlang:'not'(X).
catalog_69_qualified(X) -> erlang:'not'(X).
catalog_69_guard(X) when erlang:'not'(X) -> ok; catalog_69_guard(X) -> no.
catalog_70_body(X,Y) -> erlang:'and'(X,Y).
catalog_70_qualified(X,Y) -> erlang:'and'(X,Y).
catalog_70_guard(X,Y) when erlang:'and'(X,Y) -> ok; catalog_70_guard(X,Y) -> no.
catalog_71_body(X,Y) -> erlang:'or'(X,Y).
catalog_71_qualified(X,Y) -> erlang:'or'(X,Y).
catalog_71_guard(X,Y) when erlang:'or'(X,Y) -> ok; catalog_71_guard(X,Y) -> no.
catalog_72_body(X,Y) -> erlang:'xor'(X,Y).
catalog_72_qualified(X,Y) -> erlang:'xor'(X,Y).
catalog_72_guard(X,Y) when erlang:'xor'(X,Y) -> ok; catalog_72_guard(X,Y) -> no.
catalog_73_body(X,Y) -> erlang:'=='(X,Y).
catalog_73_qualified(X,Y) -> erlang:'=='(X,Y).
catalog_73_guard(X,Y) when erlang:'=='(X,Y) -> ok; catalog_73_guard(X,Y) -> no.
catalog_74_body(X,Y) -> erlang:'/='(X,Y).
catalog_74_qualified(X,Y) -> erlang:'/='(X,Y).
catalog_74_guard(X,Y) when erlang:'/='(X,Y) -> ok; catalog_74_guard(X,Y) -> no.
catalog_75_body(X,Y) -> erlang:'=<'(X,Y).
catalog_75_qualified(X,Y) -> erlang:'=<'(X,Y).
catalog_75_guard(X,Y) when erlang:'=<'(X,Y) -> ok; catalog_75_guard(X,Y) -> no.
catalog_76_body(X,Y) -> erlang:'<'(X,Y).
catalog_76_qualified(X,Y) -> erlang:'<'(X,Y).
catalog_76_guard(X,Y) when erlang:'<'(X,Y) -> ok; catalog_76_guard(X,Y) -> no.
catalog_77_body(X,Y) -> erlang:'>='(X,Y).
catalog_77_qualified(X,Y) -> erlang:'>='(X,Y).
catalog_77_guard(X,Y) when erlang:'>='(X,Y) -> ok; catalog_77_guard(X,Y) -> no.
catalog_78_body(X,Y) -> erlang:'>'(X,Y).
catalog_78_qualified(X,Y) -> erlang:'>'(X,Y).
catalog_78_guard(X,Y) when erlang:'>'(X,Y) -> ok; catalog_78_guard(X,Y) -> no.
catalog_79_body(X,Y) -> erlang:'=:='(X,Y).
catalog_79_qualified(X,Y) -> erlang:'=:='(X,Y).
catalog_79_guard(X,Y) when erlang:'=:='(X,Y) -> ok; catalog_79_guard(X,Y) -> no.
catalog_80_body(X,Y) -> erlang:'=/='(X,Y).
catalog_80_qualified(X,Y) -> erlang:'=/='(X,Y).
catalog_80_guard(X,Y) when erlang:'=/='(X,Y) -> ok; catalog_80_guard(X,Y) -> no.
is_integer_3_guard_1(X, LB, UB) when is_integer(X, LB, UB) ->
    true = is_integer(X, LB, UB);
is_integer_3_guard_1(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_2(X) when is_integer(X, 1, 1024) ->
    true = is_integer(X, 1, 1024);
is_integer_3_guard_2(X) ->
    is_integer(X, 1, 1024).
is_integer_3_guard_3(X) when not is_integer(X, 1, 1024) ->
    true = not is_integer(X, 1, 1024);
is_integer_3_guard_3(X) ->
    not is_integer(X, 1, 1024).
is_integer_3_guard_4(X, LB, UB) when 0 =< LB, UB < 10,
                                     is_integer(X, LB, UB) ->
    is_integer_3_guard_4_id(X);
is_integer_3_guard_4(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_5(X, LB, UB) when 0 =< LB, is_integer(UB),
                                     UB < 10, is_integer(X, LB, UB) ->
    is_integer_3_guard_5_id(X);
is_integer_3_guard_5(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_6(X, LB, UB) when 10 =< LB, UB < 0, is_integer(X, LB, UB) ->
    is_integer(X, LB, UB);
is_integer_3_guard_6(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_7(X, LB, UB) when is_number(UB), UB < 10, is_integer(X, LB, UB) ->
    is_integer(X, LB, UB);
is_integer_3_guard_7(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_8(X, LB, UB) when is_number(LB), LB > 10, is_integer(X, LB, UB) ->
    is_integer_3_guard_8_id(X),
    is_integer(X, LB, UB);
is_integer_3_guard_8(X, LB, UB) ->
    is_integer(X, LB, UB).
is_integer_3_guard_4_id(I) -> I.
is_integer_3_guard_5_id(I) -> I.
is_integer_3_guard_8_id(I) -> I.
bool_min_false(A, B) when is_boolean(A), is_boolean(B) ->
    false = min(A, B).
bool_min_true(A, B) when is_boolean(A), is_boolean(B) ->
    true = min(A, B).
bool_max_false(A, B) when is_boolean(A), is_boolean(B) ->
    false = max(A, B).
max_number(A) ->
    Res = {trunc(A), max(A, 1)},
    Res = {trunc(A), max(1, A)}.
min_increment(A) ->
    Res = min(10, A) + 1,
    Res = min(A, 10) + 1,
    Res = min(id(A), 10) + 1.
int_clamped_add(A) when is_integer(A) ->
    min(max(A, 0), 10) + 100.
num_clamped_add(A) ->
    min(max(A, 0), 10) + 100.
map_guard_empty() when is_map(#{}); false -> true.
map_guard_empty_2() when true; #{} andalso false -> true.
map_get_head(M) when map_get(a, M) =:= 1 -> true;
map_get_head(_) -> false.
map_get_head_not(M) when not map_get(a, M) -> true;
map_get_head_not(_) -> false.
map_is_key_head(M) when is_map_key(a, M) -> true;
map_is_key_head(_) -> false.
map_is_key_head_not(M) when not is_map_key(a, M) -> true;
map_is_key_head_not(_) -> false.
map_get_head_badmap1() when map_get(key, not_a_map), false -> a;
map_get_head_badmap1() -> b.
map_get_head_badmap2() when map_get(key, not_a_map), true -> a;
map_get_head_badmap2() -> b.
map_get_head_badmap3() when map_get(key, not_a_map) -> a;
map_get_head_badmap3() -> b.
map_field_check_sequence(M)
  when is_map(M) andalso is_map_key(a, M) andalso (map_get(a, M) == 1) ->
    true;
map_field_check_sequence(_) ->
    false.
id(X) -> X.
conversion_trunc(X) when trunc(X) == float(trunc(X)) -> trunc(X); conversion_trunc(_) -> no.
conversion_round(X) when round(X) == float(round(X)) -> round(X); conversion_round(_) -> no.
conversion_floor(X) when floor(X) == float(floor(X)) -> floor(X); conversion_floor(_) -> no.
conversion_ceil(X) when ceil(X) == float(ceil(X)) -> ceil(X); conversion_ceil(_) -> no.
construct(X) when element(2,{X,[X,#{a => <<1:3>>}]}) =:= [X,#{a => <<1:3>>}] -> ok; construct(_) -> no.
update(M) when map_get(a,M#{a := {1,[2],<<3:4>>}}) =:= {1,[2],<<3:4>>}; is_map(M) -> ok; update(_) -> no.
range_alt(X,L,U) when is_integer(X,L,U); X =:= a -> yes; range_alt(_,_,_) -> no.
