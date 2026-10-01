helper(First) ->
    passthrough(
        First).
passthrough(Other) ->
    answer:identity(
        Other).
