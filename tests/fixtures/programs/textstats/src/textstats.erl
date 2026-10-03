%% Command-line text and number statistics. Each argument is either free text,
%% an integer, or a malformed number; the exit status counts malformed ones.
-module(textstats).
-export([main/1]).

%% Classifies arguments, prints both reports and halts with the invalid count.
main(Args) ->
    {Words, Numbers, Invalid} = classify(Args, [], [], []),
    report_words(Words),
    report_numbers(Numbers),
    report_invalid(Invalid),
    erlang:halt(length(Invalid)).

%% Splits arguments into words, integers and malformed numbers, keeping order.
classify([], Words, Numbers, Invalid) ->
    {Words, lists:reverse(Numbers), lists:reverse(Invalid)};
classify([Arg | Rest], Words, Numbers, Invalid) ->
    case words:number(Arg) of
        {ok, N} -> classify(Rest, Words, [N | Numbers], Invalid);
        error -> classify(Rest, Words, Numbers, [Arg | Invalid]);
        text -> classify(Rest, Words ++ words:split(Arg), Numbers, Invalid)
    end.

%% Prints word totals, the most frequent words and the longest word.
report_words(Words) ->
    Counts = words:count(Words),
    io:format("words: ~b total, ~b distinct~n", [length(Words), map_size(Counts)]),
    Ranked = lists:sort([{-N, W} || W := N <- Counts]),
    print_top(Ranked, 4),
    Longest = lists:foldl(fun longer/2, "", Words),
    io:format("longest: ~s (~b letters)~n", [Longest, length(Longest)]).

%% Keeps the first of the longest words seen so far.
longer(Word, Best) when length(Word) > length(Best) -> Word;
longer(_, Best) -> Best.

%% Prints at most Limit ranked words with their counts.
print_top(_, 0) ->
    ok;
print_top([], _) ->
    ok;
print_top([{Negated, Word} | Rest], Limit) ->
    io:format("  ~s x~b~n", [Word, -Negated]),
    print_top(Rest, Limit - 1).

%% Prints sums, extremes, a two-decimal mean, signs, hex forms and big products.
report_numbers([]) ->
    io:format("numbers: none~n");
report_numbers([First | Others] = Numbers) ->
    Sum = lists:foldl(fun(N, Acc) -> N + Acc end, 0, Numbers),
    {Min, Max} = lists:foldl(fun min_max/2, {First, First}, Others),
    io:format("numbers: ~w~n", [Numbers]),
    io:format("sum ~b, min ~b, max ~b, mean ~s~n", [Sum, Min, Max, mean(Sum, length(Numbers))]),
    io:format("signs: ~w~n", [[sign(N) || N <- Numbers]]),
    io:format("hex: ~s~n", [join([integer_to_list(N, 16) || N <- Numbers])]),
    io:format("even squares: ~w~n", [[N * N || N <- Numbers, N rem 2 =:= 0]]),
    NonZero = [N || N <- Numbers, N =/= 0],
    io:format("nonzero product: ~b~n", [lists:foldl(fun(N, P) -> N * P end, 1, NonZero)]).

%% Widens the running minimum and maximum by one value.
min_max(N, {Low, High}) -> {min(N, Low), max(N, High)}.

%% Formats the arithmetic mean with two decimals.
mean(Sum, Count) -> float_to_list(Sum / Count, [{decimals, 2}]).

%% Names the sign of an integer.
sign(N) ->
    if
        N > 0 -> positive;
        N < 0 -> negative;
        true -> zero
    end.

%% Joins strings with a comma and a space.
join([]) -> "";
join([Only]) -> Only;
join([Head | Rest]) -> Head ++ ", " ++ join(Rest).

%% Prints each malformed argument in input order.
report_invalid([]) ->
    ok;
report_invalid([Arg | Rest]) ->
    io:format("invalid: ~s~n", [Arg]),
    report_invalid(Rest).
