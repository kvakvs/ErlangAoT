-module(binaries).
f(<<X:8,Rest/binary>>) -> <<X:8/integer-unsigned-big, (X+1):8, Rest/binary>>.
