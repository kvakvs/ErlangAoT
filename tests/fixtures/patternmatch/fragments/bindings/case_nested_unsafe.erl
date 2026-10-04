-module(case_nested_unsafe).
-export([f/2]).
f(A, B) ->
    case A of
        1 ->
            case B of
                1 -> X = a;
                _ -> ok
            end;
        _ ->
            X = b
    end,
    X.
