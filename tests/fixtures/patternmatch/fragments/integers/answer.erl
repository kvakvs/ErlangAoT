-module(answer).
-export([
    my_div/2,
    my_add/2,
    bnot_bounds_2_coverage/1,
    add/2,
    guard_add/2,
    subtract/2,
    guard_subtract/2,
    multiply/2,
    guard_multiply/2,
    divide/2,
    guard_divide/2,
    remainder/2,
    guard_remainder/2,
    bit_and/2,
    guard_bit_and/2,
    bit_or/2,
    guard_bit_or/2,
    bit_xor/2,
    guard_bit_xor/2,
    left/2,
    guard_left/2,
    right/2,
    guard_right/2,
    positive/1,
    negative/1,
    complement/1,
    absolute/1,
    classify/1,
    arity/1,
    element_index/1,
    legacy/1,
    qualified/2,
    roundtrip/1,
    literal/1,
    negative_literal/1,
    constant/0,
    body/1,
    repeat/2,
    independent/1,
    compare/2,
    wrong_spec/2,
    error_order/2,
    id/1
]).
my_div(Numerator, Denominator) ->
    Quotient = Numerator div Denominator,
    Quotient.
my_add(Left, Right) ->
    Sum = Left + Right,
    Sum.
bnot_bounds_2_coverage(Value) ->
    Complement = bnot Value,
    Complement.
add(X, Y) -> X + Y.
guard_add(X, Y) when (X + Y) =:= 0 -> zero;
guard_add(X, Y) when is_integer(X + Y) -> integer;
guard_add(_, _) -> rejected.
subtract(X, Y) -> X - Y.
guard_subtract(X, Y) when (X - Y) =:= 0 -> zero;
guard_subtract(X, Y) when is_integer(X - Y) -> integer;
guard_subtract(_, _) -> rejected.
multiply(X, Y) -> X * Y.
guard_multiply(X, Y) when (X * Y) =:= 0 -> zero;
guard_multiply(X, Y) when is_integer(X * Y) -> integer;
guard_multiply(_, _) -> rejected.
divide(X, Y) -> X div Y.
guard_divide(X, Y) when (X div Y) =:= 0 -> zero;
guard_divide(X, Y) when is_integer(X div Y) -> integer;
guard_divide(_, _) -> rejected.
remainder(X, Y) -> X rem Y.
guard_remainder(X, Y) when (X rem Y) =:= 0 -> zero;
guard_remainder(X, Y) when is_integer(X rem Y) -> integer;
guard_remainder(_, _) -> rejected.
bit_and(X, Y) -> X band Y.
guard_bit_and(X, Y) when (X band Y) =:= 0 -> zero;
guard_bit_and(X, Y) when is_integer(X band Y) -> integer;
guard_bit_and(_, _) -> rejected.
bit_or(X, Y) -> X bor Y.
guard_bit_or(X, Y) when (X bor Y) =:= 0 -> zero;
guard_bit_or(X, Y) when is_integer(X bor Y) -> integer;
guard_bit_or(_, _) -> rejected.
bit_xor(X, Y) -> X bxor Y.
guard_bit_xor(X, Y) when (X bxor Y) =:= 0 -> zero;
guard_bit_xor(X, Y) when is_integer(X bxor Y) -> integer;
guard_bit_xor(_, _) -> rejected.
left(X, Y) -> X bsl Y.
guard_left(X, Y) when (X bsl Y) =:= 0 -> zero;
guard_left(X, Y) when is_integer(X bsl Y) -> integer;
guard_left(_, _) -> rejected.
right(X, Y) -> X bsr Y.
guard_right(X, Y) when (X bsr Y) =:= 0 -> zero;
guard_right(X, Y) when is_integer(X bsr Y) -> integer;
guard_right(_, _) -> rejected.
positive(X) -> +X.
negative(X) -> -X.
complement(X) -> bnot X.
absolute(X) -> abs(X).
classify(X) -> {is_integer(X), is_number(X), is_float(X)}.
arity(X) -> is_function(ok, X).
element_index(X) -> element(X, {a, b}).
legacy(X) when integer(X) -> X;
legacy(_) -> no.
qualified(X, Y) -> erlang:'+'(X, Y).
roundtrip(X) ->
    A = (X bsl 130) + 7,
    (A - 7) bsr 130.
literal(1361129467683753853853498429727072845824) -> yes;
literal(_) -> no.
negative_literal(-1361129467683753853853498429727072845824) -> yes;
negative_literal(_) -> no.
constant() -> {1361129467683753853853498429727072845824, -1361129467683753853853498429727072845824}.
body(X) ->
    1361129467683753853853498429727072845824 = X,
    X.
repeat(X, X) -> same;
repeat(_, _) -> different.
independent(X) ->
    A = {X bsl 130, [X bsl 131]},
    B = {X bsl 130, [X bsl 131]},
    A = B,
    A =:= B.
compare(X, Y) -> {X =:= Y, X == Y, X < Y, X =< Y, X > Y, X >= Y, min(X, Y), max(X, Y)}.
-spec wrong_spec(integer(), integer()) -> integer().
wrong_spec(X, Y) -> X + Y.
error_order(X, Y) -> {X div Y, ok = impossible}.
id(Value) ->
    Saved = Value,
    Saved.
