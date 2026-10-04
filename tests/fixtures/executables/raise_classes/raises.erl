-module(raises).
-export([main/1]).

%% The first argument selects which exception escapes the entry, its spelling and call depth.
main(["error"]) ->
    error(oops);
main(["error_term"]) ->
    erlang:error({bad, [1, 2.5, <<"bin">>], #{key => value}, 123456789012345678901234567890});
main(["error2"]) ->
    error(with_args, [a, b]);
main(["error3"]) ->
    erlang:error(with_info, [x], [{error_info, #{module => raises}}]);
main(["throw"]) ->
    throw(ball);
main(["exit"]) ->
    erlang:exit(shutdown);
main(["exit_normal"]) ->
    exit(normal);
main(["error_deep"]) ->
    Result = depth:one(error, deep),
    erlang:display({unreachable, Result});
main(["throw_deep"]) ->
    Result = depth:one(throw, {ball, 3}),
    erlang:display({unreachable, Result});
main(["exit_deep"]) ->
    Result = depth:one(exit, "stop"),
    erlang:display({unreachable, Result});
main(["local"]) ->
    erlang:display(local),
    first(throw);
main(["order"]) ->
    {erlang:display(first), throw(second), erlang:display(never)};
main(["case"]) ->
    case depth:pick(2) of
        two -> exit({case_body, two});
        Other -> Other
    end;
main(["guarded"]) ->
    erlang:display(shadow:error(local_error)),
    shadow:fail(remote_error);
main(Args) ->
    erlang:display(Args).

%% Local calls carry an exception unchanged through every caller.
first(Class) -> second(Class, [first]).

second(Class, Path) -> third(Class, [second | Path]).

third(throw, Path) -> throw({local, Path}).
