% Token spellings and values, including OTP 29 operators.
-module(sample).
value(X, _Name) -> {'a b', X#record.field, 1.25e-2, 16#ff, 2#1010,
    123456789012345678901234567890, 1_000, $\n, $\x{1F600}, "λ", "?MACRO"}.
ops() -> =:= =/= <:- <:= ... .. && ?= << <- <= >> >= -> -- ++ =< => == /= || := :: ?? .
words() -> after begin case try cond catch andalso orelse end fun if let of receive when
    bnot not div rem band and bor bxor bsl bsr or xor maybe else.
