-module(binaries).
-define(SEG, X:Size/integer-signed-little-unit:8).
f(<<H:8, T/binary>>, <<(g()):N/custom-custom>>) ->
    {<<>>, <<1, 2:3, X:N, X:default/default>>, <<?SEG>>, <<(A + B):(N * 8)/unsigned-big-integer>>,
     <<+X, -42, bnot X, not X, -(A + B)>>,
     <<"hé" "λ", "λ"/utf8, "text":8>>, <<<<1,2>>/binary>>, <<(f()):(size(X))/binary>>,
     <<(#{a => 1})/binary, (#r{})/binary, {a,b}, [a,b], (M:F(X))>>,
     <<X/foo-bar:0-baz:999999999999999999999999999999999999-foo>>,
     <<X/integer-integer-signed-unsigned-big-little-native-unit:8-unit:1>>,
     <<A:B/integer, C:(D / E)/unit:8>>, <<A/b>>, <<X:1.5>>, <<X:"size">>,
     <<"λ"/utf8>>, <<("λ")/utf8>>, ~b"λ", ~B"\n", ~"λ",
     <<~b"λ"/binary>>, <<1>>#{a => 2}, <<1>>#r.a, <<A>> =:= <<B>>}.
