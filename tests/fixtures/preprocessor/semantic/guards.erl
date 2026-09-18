-module(guards).
-if(abs(-9) =:= 9).
selected(0,true).
-else.
selected(0,false).
-endif.
-if(abs(-1.5) =:= 1.5).
selected(1,true).
-else.
selected(1,false).
-endif.
-if(ceil(1.1) =:= 2).
selected(2,true).
-else.
selected(2,false).
-endif.
-if(floor(-1.1) =:= -2).
selected(3,true).
-else.
selected(3,false).
-endif.
-if(round(-1.5) =:= -2).
selected(4,true).
-else.
selected(4,false).
-endif.
-if(trunc(-1.9) =:= -1).
selected(5,true).
-else.
selected(5,false).
-endif.
-if(float(1) =:= 1.0).
selected(6,true).
-else.
selected(6,false).
-endif.
-if(length([1,2]) =:= 2).
selected(7,true).
-else.
selected(7,false).
-endif.
-if(hd([1|2]) =:= 1).
selected(8,true).
-else.
selected(8,false).
-endif.
-if(tl([1|2]) =:= 2).
selected(9,true).
-else.
selected(9,false).
-endif.
-if(is_list([1|2])).
selected(10,true).
-else.
selected(10,false).
-endif.
-if(size({a,b}) =:= 2).
selected(11,true).
-else.
selected(11,false).
-endif.
-if(tuple_size({}) =:= 0).
selected(12,true).
-else.
selected(12,false).
-endif.
-if(element(1,{a}) =:= a).
selected(13,true).
-else.
selected(13,false).
-endif.
-if(map_get(1,#{1=>a,1.0=>b}) =:= a).
selected(14,true).
-else.
selected(14,false).
-endif.
-if(map_size(#{1=>a,1.0=>b}) =:= 2).
selected(15,true).
-else.
selected(15,false).
-endif.
-if(is_map_key(a,#{a=>1})).
selected(16,true).
-else.
selected(16,false).
-endif.
-if(#{a=>1}#{a:=2} =:= #{a=>2}).
selected(17,true).
-else.
selected(17,false).
-endif.
-if(bit_size(<<3:5>>) =:= 5).
selected(18,true).
-else.
selected(18,false).
-endif.
-if(byte_size(<<3:5>>) =:= 1).
selected(19,true).
-else.
selected(19,false).
-endif.
-if(size(<<3:5>>) =:= 0).
selected(20,true).
-else.
selected(20,false).
-endif.
-if(binary_part(<<1,2,3>>, {1,2}) =:= <<2,3>>).
selected(21,true).
-else.
selected(21,false).
-endif.
-if(binary_part(<<1,2,3>>, 3,-2) =:= <<2,3>>).
selected(22,true).
-else.
selected(22,false).
-endif.
-if(<<16#123:12/little>> =:= <<16#23,1:4>>).
selected(23,true).
-else.
selected(23,false).
-endif.
-if(<<65/utf8>> =:= <<65>>).
selected(24,true).
-else.
selected(24,false).
-endif.
-if(<<16#10000/utf16>> =:= <<16#d800:16,16#dc00:16>>).
selected(25,true).
-else.
selected(25,false).
-endif.
-if(<<1.0:16/float>> =:= <<16#3c00:16>>).
selected(26,true).
-else.
selected(26,false).
-endif.
-if(<<1.0:32/float>> =:= <<16#3f800000:32>>).
selected(27,true).
-else.
selected(27,false).
-endif.
-if(is_atom(a)).
selected(28,true).
-else.
selected(28,false).
-endif.
-if(is_boolean(false)).
selected(29,true).
-else.
selected(29,false).
-endif.
-if(is_binary(<<1>>)).
selected(30,true).
-else.
selected(30,false).
-endif.
-if(is_bitstring(<<1:1>>)).
selected(31,true).
-else.
selected(31,false).
-endif.
-if(is_float(1.0)).
selected(32,true).
-else.
selected(32,false).
-endif.
-if(is_integer(1)).
selected(33,true).
-else.
selected(33,false).
-endif.
-if(is_integer(3,1,4)).
selected(34,true).
-else.
selected(34,false).
-endif.
-if(is_integer(3,1.0,4.0)).
selected(35,true).
-else.
selected(35,false).
-endif.
-if(is_integer(3,a,z)).
selected(36,true).
-else.
selected(36,false).
-endif.
-if(is_map(#{})).
selected(37,true).
-else.
selected(37,false).
-endif.
-if(is_number(1.0)).
selected(38,true).
-else.
selected(38,false).
-endif.
-if(is_pid(self())).
selected(39,true).
-else.
selected(39,false).
-endif.
-if(is_port(self())).
selected(40,true).
-else.
selected(40,false).
-endif.
-if(is_reference(self())).
selected(41,true).
-else.
selected(41,false).
-endif.
-if(is_tuple({})).
selected(42,true).
-else.
selected(42,false).
-endif.
-if(is_function(a)).
selected(43,true).
-else.
selected(43,false).
-endif.
-if(is_function(a,256)).
selected(44,true).
-else.
selected(44,false).
-endif.
-if(not is_function(a,256)).
selected(45,true).
-else.
selected(45,false).
-endif.
-if(is_record({r,a},r,2)).
selected(46,true).
-else.
selected(46,false).
-endif.
-if(is_record({r,a},r)).
selected(47,true).
-else.
selected(47,false).
-endif.
-if(is_record({r,a})).
selected(48,true).
-else.
selected(48,false).
-endif.
-if(is_record({r,a},r,0)).
selected(49,true).
-else.
selected(49,false).
-endif.
-if(min(1,1.0) =:= 1).
selected(50,true).
-else.
selected(50,false).
-endif.
-if(max(1,1.0) =:= 1).
selected(51,true).
-else.
selected(51,false).
-endif.
-if(node(self()) =:= node()).
selected(52,true).
-else.
selected(52,false).
-endif.
-if(erlang:'+'(1,2) =:= 3).
selected(53,true).
-else.
selected(53,false).
-endif.
-if(erlang:'not'(false)).
selected(54,true).
-else.
selected(54,false).
-endif.
-if('+'(1,2) =:= 3).
selected(55,true).
-else.
selected(55,false).
-endif.
-if(erlang:defined(MODULE)).
selected(56,true).
-else.
selected(56,false).
-endif.
-if((1 bsl 80)+1 > (1 bsl 80)).
selected(57,true).
-else.
selected(57,false).
-endif.
-if(-7 div 3 =:= -2).
selected(58,true).
-else.
selected(58,false).
-endif.
-if(-7 rem 3 =:= -1).
selected(59,true).
-else.
selected(59,false).
-endif.
-if((1 bsl -2) =:= 0).
selected(60,true).
-else.
selected(60,false).
-endif.
-if((-7 bsr 2) =:= -2).
selected(61,true).
-else.
selected(61,false).
-endif.
-if((bnot 3) =:= -4).
selected(62,true).
-else.
selected(62,false).
-endif.
-if(1 == 1.0).
selected(63,true).
-else.
selected(63,false).
-endif.
-if(1 =:= 1.0).
selected(64,true).
-else.
selected(64,false).
-endif.
-if({1} == {1.0}).
selected(65,true).
-else.
selected(65,false).
-endif.
-if([1] == [1.0]).
selected(66,true).
-else.
selected(66,false).
-endif.
-if(#{1=>x} == #{1.0=>x}).
selected(67,true).
-else.
selected(67,false).
-endif.
-if(#{a=>1} == #{a=>1.0}).
selected(68,true).
-else.
selected(68,false).
-endif.
-if([] < [0]).
selected(69,true).
-else.
selected(69,false).
-endif.
-if({a} < {0,0}).
selected(70,true).
-else.
selected(70,false).
-endif.
-if(9007199254740993 > 9007199254740992.0).
selected(71,true).
-else.
selected(71,false).
-endif.
-if(1 < 2 < 3).
selected(72,true).
-else.
selected(72,false).
-endif.
-if(true orelse missing()).
selected(73,true).
-else.
selected(73,false).
-endif.
-if(true orelse <<1/unknown>>).
selected(74,true).
-else.
selected(74,false).
-endif.
-if(true orelse X).
selected(75,true).
-else.
selected(75,false).
-endif.
-if(true orelse (1 div 0)).
selected(76,true).
-else.
selected(76,false).
-endif.
-if(is_tuple({r,1})).
selected(77,true).
-else.
selected(77,false).
-endif.
-if(true orelse #r.a).
selected(78,true).
-else.
selected(78,false).
-endif.
-if(true orelse X#r.a).
selected(79,true).
-else.
selected(79,false).
-endif.
-if(true orelse X#r.a).
selected(80,true).
-else.
selected(80,false).
-endif.
-if(~s"abc" =:= "abc").
selected(81,true).
-else.
selected(81,false).
-endif.
-if(~b"å" =:= <<195,165>>).
selected(82,true).
-else.
selected(82,false).
-endif.
-if(~"abc" =:= <<97,98,99>>).
selected(83,true).
-else.
selected(83,false).
-endif.
-if(~s"a"bad =:= "a").
selected(84,true).
-else.
selected(84,false).
-endif.
-if(is_function(fun erlang:length/1)).
selected(85,true).
-else.
selected(85,false).
-endif.
-if(<<1:8/unit:2>> =:= <<0,1>>).
selected(86,true).
-else.
selected(86,false).
-endif.
-if(<<1/integer-float>> =:= <<1>>).
selected(87,true).
-else.
selected(87,false).
-endif.
-if(<<1:2/utf8>> =:= <<1>>).
selected(88,true).
-else.
selected(88,false).
-endif.
-if(true orelse <<1:2/utf8>>).
selected(89,true).
-else.
selected(89,false).
-endif.
-if(true orelse <<1/unit:0>>).
selected(90,true).
-else.
selected(90,false).
-endif.
-if(#{2=>a} < #{1.0=>a}). map_key_order(true). -else. map_key_order(false). -endif.
-if(<<[]>> =:= <<>>). empty_segment(true). -else. empty_segment(false). -endif.
-if(1 / (1 bsl 2000) =:= 0.0). float_overflow(true). -else. float_overflow(false). -endif.
