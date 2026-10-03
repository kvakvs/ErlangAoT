%% Binary protocol demo: encodes messages, decodes valid, corrupted and
%% truncated streams, and inspects sub-byte bitstrings.
-module(frames).
-export([main/1]).

%% Runs every decoding scenario and prints the outcomes.
main(_Args) ->
    Messages = [
        {text, <<"hello">>},
        {ints, [1, -2, 70000, -2147483648, 2147483647]},
        {pair, retries, 513},
        {text, <<"caf", 16#E9/utf8, " ", 16#4E16/utf8>>},
        {ints, []}
    ],
    Stream = codec:encode(Messages),
    io:format("encoded ~b messages into ~b bytes~n", [length(Messages), byte_size(Stream)]),
    io:format("stream: ~w~n", [Stream]),
    show(valid, codec:decode(Stream)),
    show(corrupted, codec:decode(flip(Stream, 5))),
    <<Prefix:20/binary, _/binary>> = Stream,
    show(truncated, codec:decode(Prefix)),
    show(unknown, codec:decode(<<16#40, 9, 0, 1, 7, 7>>)),
    show(bad_utf8, codec:decode(<<16#40, 1, 0, 1, 16#FF, 16#FF>>)),
    show(empty, codec:decode(<<>>)),
    {ok, _, Decoded} = codec:decode(Stream),
    Again = codec:encode([strip(M) || M <- Decoded]),
    io:format("round trip equal: ~w~n", [Again =:= Stream]),
    bits(Stream).

%% Prints a labelled decode result one message per line.
show(Label, {ok, Header, Messages}) ->
    io:format("~w: header ~w~n", [Label, Header]),
    [io:format("  ~w~n", [M]) || M <- Messages],
    ok;
show(Label, {error, Reason}) ->
    io:format("~w: error ~w~n", [Label, Reason]).

%% Drops decoder-only annotations so messages can be re-encoded.
strip({text, Text, _Length}) -> {text, Text};
strip(Message) -> Message.

%% Inverts every bit of the byte at Position.
flip(Bin, Position) ->
    <<Before:Position/binary, Byte, After/binary>> = Bin,
    <<Before/binary, (Byte bxor 16#FF), After/binary>>.

%% Shows partial-byte bitstrings built from the header and first frame.
bits(<<Header:3/bitstring, Rest/bitstring>>) ->
    Nibbles = <<<<(N band 15):4>> || <<N>> <= binary_part_of(Rest, 3)>>,
    io:format("header bits: ~w (~b bits)~n", [Header, bit_size(Header)]),
    io:format("nibbles: ~w (~b bits)~n", [Nibbles, bit_size(Nibbles)]),
    io:format("parity: ~w~n", [<<<<(B rem 2):1>> || <<B>> <= binary_part_of(Rest, 8)>>]).

%% Returns the first Count whole bytes after skipping the 5 remaining header bits.
binary_part_of(Bits, Count) ->
    <<_:5, Bytes:Count/binary, _/bitstring>> = Bits,
    Bytes.
