-module(answer).
-export([
    bin_tail_c/2,
    bin_tail_c_dead/2,
    bin_tail_c_var/2,
    bin_tail_d_dead/2,
    bin_tail_d_var/2,
    build/2,
    little/2,
    native/2,
    unit/2,
    unit256/1,
    read_unit256/1,
    bytes/1,
    clone/1,
    join/2,
    prefix/2,
    all_explicit/1,
    all_pattern/1,
    float_literal/1,
    map_key/2,
    float_be/2,
    float_le/2,
    utf8/1,
    utf16/1,
    utf16le/1,
    utf32/1,
    literal/0,
    empty_string/1,
    strings/1,
    head/1,
    zero/1,
    overfull/1,
    dependent/1,
    repeat/1,
    bound/3,
    read/2,
    read_le/2,
    read_signed/2,
    read_signed_le/2,
    read_native/2,
    read_float/2,
    read_float_le/2,
    get_utf8/1,
    get_utf16/1,
    get_utf16le/1,
    get_utf32/1,
    literal_head/1,
    utf_literal/1,
    binary_tail/1,
    all_unit/1,
    body/1,
    shared/1,
    classify/1,
    sizes/1,
    part/3,
    part_tuple/2,
    guard_part/3,
    guard_build/2,
    size_failure/1,
    guard_alternative/1,
    compare/2,
    alias/1,
    duplicate/2,
    wrong_spec/1,
    ordered/1,
    id/1
]).
bin_tail_c(Bytes, Skip) ->
    {Byte, Rest} = bin_tail_c_var(Bytes, Skip),
    Byte = bin_tail_c_dead(Bytes, Skip),
    <<_:Skip/binary, Byte, Rest/binary>> = Bytes,
    Byte.
bin_tail_c_dead(Bytes, Skip) ->
    <<_:Skip/binary, Selected, _Rest/binary>> = Bytes,
    Saved = Selected,
    Saved.
bin_tail_c_var(Bytes, Skip) ->
    <<_:Skip/binary, Selected, Rest/binary>> = Bytes,
    Result = {Selected, Rest},
    Result.
bin_tail_d_dead(Bytes, SkipBits) ->
    <<_:SkipBits/bitstring, Selected, _Rest/binary>> = Bytes,
    Saved = Selected,
    Saved.
bin_tail_d_var(Bytes, SkipBits) ->
    <<_:SkipBits/bitstring, Selected, Rest/binary>> = Bytes,
    Result = {Selected, Rest},
    Result.
build(X, N) -> <<X:N>>.
little(X, N) -> <<X:N/little>>.
native(X, N) -> <<X:N/native>>.
unit(X, N) -> <<X:N/signed-little-unit:7>>.
unit256(X) -> <<X:1/unit:256>>.
read_unit256(<<X:1/unit:256, T/bits>>) -> {X, T};
read_unit256(_) -> no.
bytes(B) -> <<B/bytes>>.
clone(B) -> <<B/bits>>.
join(A, B) -> <<A/bitstring, B/bitstring>>.
prefix(X, B) -> <<X:3, B/bitstring, X:5>>.
all_explicit(B) -> <<B:all/binary>>.
all_pattern(<<T:all/binary>>) -> T;
all_pattern(_) -> no.
float_literal(<<1:16/float, T/bitstring>>) -> {integer, T};
float_literal(<<1.00146484375:16/float, T/bitstring>>) -> {rounded, T};
float_literal(<<-0.0:32/float, T/bitstring>>) -> {negative, T};
float_literal(_) -> no.
map_key(K, B) ->
    #{K := V} = #{B => B},
    V.
float_be(X, N) -> <<X:N/float>>.
float_le(X, N) -> <<X:N/float-little>>.
utf8(X) -> <<X/utf8>>.
utf16(X) -> <<X/utf16>>.
utf16le(X) -> <<X/utf16-little>>.
utf32(X) -> <<X/utf32-native>>.
literal() -> <<"abc":16, "Ω𐀀"/utf8, 0:3>>.
empty_string(N) -> <<"":N, 5:3>>.
strings(N) -> <<"abc":N>>.
head(<<A:3, B:5, T/bitstring>>) -> {A, B, T};
head(_) -> no.
zero(<<42>>) -> star;
zero(<<V:0>>) -> V;
zero(_) -> no_match.
overfull(<<256:8>>) -> impossible;
overfull(_) -> no.
dependent(<<N:8, T:N/binary, R/bitstring>>) -> {N, T, R};
dependent(_) -> no.
repeat(<<X:8, X:8, T/bitstring>>) -> {X, T};
repeat(_) -> no.
bound(N, X, B) ->
    <<X:N, T/bitstring>> = B,
    T.
read(N, B) ->
    <<X:N, T/bitstring>> = B,
    {X, T}.
read_le(N, B) ->
    <<X:N/little, T/bitstring>> = B,
    {X, T}.
read_signed(N, B) ->
    <<X:N/signed, T/bitstring>> = B,
    {X, T}.
read_signed_le(N, B) ->
    <<X:N/signed-little, T/bitstring>> = B,
    {X, T}.
read_native(N, B) ->
    <<X:N/native, T/bitstring>> = B,
    {X, T}.
read_float(N, B) ->
    <<X:N/float, T/bitstring>> = B,
    {X, T}.
read_float_le(N, B) ->
    <<X:N/float-little, T/bitstring>> = B,
    {X, T}.
get_utf8(<<X/utf8, T/bitstring>>) -> {X, T};
get_utf8(_) -> no.
get_utf16(<<X/utf16, T/bitstring>>) -> {X, T};
get_utf16(_) -> no.
get_utf16le(<<X/utf16-little, T/bitstring>>) -> {X, T};
get_utf16le(_) -> no.
get_utf32(<<X/utf32-native, T/bitstring>>) -> {X, T};
get_utf32(_) -> no.
literal_head(<<"abc", T/bitstring>>) -> T;
literal_head(_) -> no.
utf_literal(<<"Ω𐀀"/utf8, T/bitstring>>) -> T;
utf_literal(_) -> no.
binary_tail(<<_:1, T/binary>>) -> T;
binary_tail(_) -> no.
all_unit(<<T/binary-unit:3>>) -> T;
all_unit(_) -> no.
body(B) ->
    <<1:3, T/bitstring>> = B,
    T.
shared(<<_, T/bitstring>>) ->
    <<_, R/bitstring>> = T,
    {T, R};
shared(_) ->
    no.
classify(B) -> {is_binary(B), is_bitstring(B), is_list(B), is_tuple(B)}.
sizes(B) -> {size(B), byte_size(B), bit_size(B)}.
part(B, S, N) -> binary_part(B, S, N).
part_tuple(B, P) -> erlang:binary_part(B, P).
guard_part(B, S, N) when byte_size(binary_part(B, S, N)) >= 0 -> yes;
guard_part(_, _, _) -> no.
guard_build(X, N) when bit_size(<<X:N>>) > 0 -> yes;
guard_build(_, _) -> no.
size_failure(<<X:(1 div 0)>>) -> X;
size_failure(_) -> recovered.
guard_alternative(X) when bit_size(<<X/utf8>>) > 0; is_atom(X) -> yes;
guard_alternative(_) -> no.
compare(A, B) -> {A =:= B, A == B, A < B, A =< B, A > B, A >= B}.
alias(<<1, T/bitstring>> = B) -> {B, T};
alias(_) -> no.
duplicate(B, B) -> same;
duplicate(_, _) -> different.
-spec wrong_spec(integer()) -> integer() | tuple().
wrong_spec(<<X, T/binary>>) -> {X, T};
wrong_spec(_) -> no.
ordered(X) -> <<X:0, (1 div 0)>>.
id(Value) ->
    Saved = Value,
    Saved.
