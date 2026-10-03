-module(answer).
-export([
    bool/1,
    csemi4_orelse_a/4,
    csemi4_orelse_b/4,
    csemi4_orelse_c/4,
    csemi4_orelse_d/4,
    char_alias_1/1,
    overlap/2,
    rollback/2,
    failed_head/2,
    alias/2,
    errors/1,
    exhaust/1,
    body_failure/1,
    local/1,
    later/1,
    common/1,
    join/1,
    wrong_spec/2,
    wide/1
]).
bool(true) -> ok;
bool(false) -> ok;
bool(_) -> error.
csemi4_orelse_a(Left, LeftLimit, Right, RightLimit) when
    (tuple_size(Left) >= 2 orelse 1 < LeftLimit); (tuple_size(Right) >= 2 orelse 1 < RightLimit)
->
    ok;
csemi4_orelse_a(_, _, _, _) ->
    error.
csemi4_orelse_b(Left, LeftLimit, Right, RightLimit) when
    (1 < LeftLimit orelse tuple_size(Left) >= 2); (tuple_size(Right) >= 2 orelse 1 < RightLimit)
->
    ok;
csemi4_orelse_b(_, _, _, _) ->
    error.
csemi4_orelse_c(Left, LeftLimit, Right, RightLimit) when
    (tuple_size(Left) >= 2 orelse 1 < LeftLimit); (1 < RightLimit orelse tuple_size(Right) >= 2)
->
    ok;
csemi4_orelse_c(_, _, _, _) ->
    error.
csemi4_orelse_d(Left, LeftLimit, Right, RightLimit) when
    ((1 < LeftLimit) or (tuple_size(Left) >= 2)); ((1 < RightLimit) or (tuple_size(Right) >= 2))
->
    ok;
csemi4_orelse_d(_, _, _, _) ->
    error.
char_alias_1($v = 118 = Selected) ->
    _ = Selected,
    ok;
char_alias_1(119 = $w = Selected) ->
    _ = Selected,
    ok;
char_alias_1(42 = Selected) ->
    42 = Selected,
    ok;
char_alias_1(_) ->
    error.
overlap(X, X) -> repeated;
overlap(0, _) -> zero;
overlap(_, X) -> X.
rollback(X, Y) when X =:= Y -> X;
rollback(Y, X) -> X.
failed_head(X, X) -> same;
failed_head(_, X) -> X.
alias(X = Y, Z) when Y =:= Z -> X;
alias(_, X) -> X.
errors(X) when hd(X) -> impossible;
errors(X) when is_atom(X) -> X;
errors(_) -> fallback.
exhaust(0) -> zero;
exhaust(X) when X =:= ok -> X.
body_failure(X) when is_integer(X) -> hd([]);
body_failure(_) -> unreachable.
local(0) -> first;
local(X) -> later(X).
later(X) -> X.
common(0 = X) -> X;
common(X) -> X.
join(0) -> 1;
join(_) -> 2.
-spec wrong_spec(integer(), integer()) -> integer().
wrong_spec(X, X) -> X;
wrong_spec(_, X) -> X.
wide(0) -> 0;
wide(1) -> 1;
wide(2) -> 2;
wide(3) -> 3;
wide(4) -> 4;
wide(5) -> 5;
wide(6) -> 6;
wide(7) -> 7;
wide(8) -> 8;
wide(9) -> 9;
wide(10) -> 10;
wide(11) -> 11;
wide(12) -> 12;
wide(13) -> 13;
wide(14) -> 14;
wide(15) -> 15;
wide(16) -> 16;
wide(17) -> 17;
wide(18) -> 18;
wide(19) -> 19;
wide(20) -> 20;
wide(21) -> 21;
wide(22) -> 22;
wide(23) -> 23;
wide(24) -> 24;
wide(25) -> 25;
wide(26) -> 26;
wide(27) -> 27;
wide(28) -> 28;
wide(29) -> 29;
wide(30) -> 30;
wide(31) -> 31;
wide(32) -> 32;
wide(33) -> 33;
wide(34) -> 34;
wide(35) -> 35;
wide(36) -> 36;
wide(37) -> 37;
wide(38) -> 38;
wide(39) -> 39;
wide(40) -> 40;
wide(41) -> 41;
wide(42) -> 42;
wide(43) -> 43;
wide(44) -> 44;
wide(45) -> 45;
wide(46) -> 46;
wide(47) -> 47;
wide(48) -> 48;
wide(49) -> 49;
wide(50) -> 50;
wide(51) -> 51;
wide(52) -> 52;
wide(53) -> 53;
wide(54) -> 54;
wide(55) -> 55;
wide(56) -> 56;
wide(57) -> 57;
wide(58) -> 58;
wide(59) -> 59;
wide(60) -> 60;
wide(61) -> 61;
wide(62) -> 62;
wide(63) -> 63;
wide(64) -> 64;
wide(65) -> 65;
wide(66) -> 66;
wide(67) -> 67;
wide(68) -> 68;
wide(69) -> 69;
wide(70) -> 70;
wide(71) -> 71;
wide(72) -> 72;
wide(73) -> 73;
wide(74) -> 74;
wide(75) -> 75;
wide(76) -> 76;
wide(77) -> 77;
wide(78) -> 78;
wide(79) -> 79;
wide(80) -> 80;
wide(81) -> 81;
wide(82) -> 82;
wide(83) -> 83;
wide(84) -> 84;
wide(85) -> 85;
wide(86) -> 86;
wide(87) -> 87;
wide(88) -> 88;
wide(89) -> 89;
wide(90) -> 90;
wide(91) -> 91;
wide(92) -> 92;
wide(93) -> 93;
wide(94) -> 94;
wide(95) -> 95;
wide(96) -> 96;
wide(97) -> 97;
wide(98) -> 98;
wide(99) -> 99;
wide(100) -> 100;
wide(101) -> 101;
wide(102) -> 102;
wide(103) -> 103;
wide(104) -> 104;
wide(105) -> 105;
wide(106) -> 106;
wide(107) -> 107;
wide(108) -> 108;
wide(109) -> 109;
wide(110) -> 110;
wide(111) -> 111;
wide(112) -> 112;
wide(113) -> 113;
wide(114) -> 114;
wide(115) -> 115;
wide(116) -> 116;
wide(117) -> 117;
wide(118) -> 118;
wide(119) -> 119;
wide(120) -> 120;
wide(121) -> 121;
wide(122) -> 122;
wide(123) -> 123;
wide(124) -> 124;
wide(125) -> 125;
wide(126) -> 126;
wide(127) -> 127;
wide(X) -> X.
