-ifndef(LOCAL_CHECKS_HRL).
-define(LOCAL_CHECKS_HRL, true).
-define(CHECK(Condition),
    (fun() ->
        case (Condition) of
            true -> ok;
            Other -> erlang:error({check_failed, ??Condition, Other})
        end
    end)()
).
-define(EQUAL(Expected, Actual),
    (fun() ->
        Pair = {(Expected), (Actual)},
        case Pair of
            {Same, Same} -> ok;
            Other -> erlang:error({unequal, Other})
        end
    end)()
).
-define(MATCH(Pattern, Value),
    (fun() ->
        case (Value) of
            Pattern -> ok;
            Other -> erlang:error({shape, Other})
        end
    end)()
).
-define(RAISES(Class, Reason, Expression),
    (fun() ->
        try (Expression) of
            Value -> erlang:error({returned, Value})
        catch
            Class:Reason -> ok
        end
    end)()
).
-endif.
