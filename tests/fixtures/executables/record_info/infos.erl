-module(infos).
-export([main/1]).

-include("shapes.hrl").

-record(empty, {}).
-record(person, {name, age = 0, email = undefined, tags = []}).
%% Defaults may use record_info/2 of an earlier record.
-record(schema, {fields = record_info(fields, person), size = record_info(size, person)}).

%% record_info/2 expands at compile time to the declared field names or the tuple size.
main(_) ->
    erlang:display({record_info(fields, empty), record_info(size, empty)}),
    erlang:display({record_info(fields, person), record_info(size, person)}),
    erlang:display({record_info(fields, shape), record_info(size, shape)}),
    erlang:display(#schema{}),
    erlang:display(record_info((fields), (person))),
    %% The size is the tuple arity, which is the tuple index of the last field.
    erlang:display({
        record_info(size, person) =:= #person.tags, record_info(size, person) =:= #person.tags + 1
    }),
    erlang:display([Field || Field <- record_info(fields, person), Field =/= name]),
    erlang:display(
        case record_info(size, shape) of
            3 -> three;
            _ -> other
        end
    ).
