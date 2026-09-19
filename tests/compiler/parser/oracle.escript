#!/usr/bin/env escript
%% Produce private normalized abstract forms, never a public stage format.
main([Mode, Input]) ->
    case list_to_integer(erlang:system_info(otp_release)) >= 29 of
        true -> run(Mode, Input);
        false -> io:format(standard_error, "OTP 29 or newer is required~n", []), halt(1)
    end.

run("raw", Input) ->
    {ok, File} = file:open(Input, [read, {encoding, utf8}]),
    try raw(File, {1,1}) after file:close(File) end;
run("epp", Input) -> with_epp(Input, fun expanded/1);
run("accept", Input) ->
    {ok, Forms} = epp:parse_file(Input, [], []),
    Failed = lists:any(fun({error, _}) -> true; (_) -> false end, Forms),
    io:format("~s~n", [case Failed of true -> "rejected"; false -> "accepted" end]);
run("phase1", Input) -> with_epp(Input, fun phase1/1);
run("lint", Input) ->
    {ok, Forms} = epp:parse_file(Input, [], []),
    Result = case erl_lint:module(Forms, Input) of
        {ok, _} -> ok;
        {error, _, _} -> error
    end,
    emit({lint, Result}).

raw(File, Location) ->
    case io:scan_erl_form(File, '', Location) of
        {ok, Tokens, Next} -> result(erl_parse:parse_form(Tokens)), raw(File, Next);
        {error, Error, Next} -> result({error, Error}), raw(File, Next);
        {eof, _} -> ok
    end.

expanded(Epp) ->
    case epp:parse_erl_form(Epp) of
        {eof, _} -> ok;
        Event -> result(Event), expanded(Epp)
    end.

result({ok, Form0}) ->
    Form = erl_parse:map_anno(fun(_) -> 0 end, Form0),
    emit({ok, normalize_file(Form)});
result({error, {Location, Module, _Description}}) -> emit({error, Module, Location});
result({warning, {Location, Module, _Description}}) -> emit({warning, Module, Location}).

normalize_file({attribute, A, file, {Path, Line}}) ->
    {attribute, A, file, {filename:basename(Path), Line}};
normalize_file(Form) -> Form.

%% Preserve binary64 bits and map keys; annotations alone are normalized away.
stable(Value) when is_float(Value) -> {float_bits, binary:encode_hex(<<Value:64/float>>, lowercase)};
stable(Value) when is_tuple(Value) -> list_to_tuple([stable(X) || X <- tuple_to_list(Value)]);
stable([H|T]) -> [stable(H)|stable(T)];
stable(Value) when is_map(Value) -> {map_value, lists:sort([{stable(K), stable(V)} || K := V <- Value])};
stable(Value) -> Value.

emit(Value) -> io:format("~0tp.~n", [stable(Value)]).

%% Keep the Phase I projection deliberately closed: unknown forms fail the oracle.
with_epp(Input, Consumer) ->
    {ok, Epp} = epp:open([{name, Input}, {location, {1,1}}]),
    try Consumer(Epp) after epp:close(Epp) end.

phase1(Epp) ->
    case epp:parse_erl_form(Epp) of
        {eof, _} -> ok;
        {ok, Form} -> project(Form), phase1(Epp);
        Other -> erlang:error({unexpected_phase1_event, Other})
    end.

project({attribute, _, module, Name}) -> field("module", atom_to_list(Name));
project({attribute, _, file, {Name, Line}}) ->
    io:format("file\t~s\t~B~n", [hex(filename:basename(Name)), Line]);
project({function, _, Name, 0, [{clause, _, [], [], [Expr]}]}) ->
    field("function", atom_to_list(Name)), scalar(Expr);
project({function, _, Name, Arity, Clauses}) ->
    io:format("function_full\t~s\t~B\t~B~n", [hex(atom_to_list(Name)), Arity, length(Clauses)]),
    lists:foreach(fun clause/1, Clauses);
project(Other) -> erlang:error({unmapped_phase1_form, Other}).

scalar({map, _, Fields}) -> map(none, Fields);
scalar({'fun', _, {function, Name, Arity}}) ->
    io:format("local_fun~n"), scalar({atom,0,Name}), scalar({integer,0,Arity});
scalar({'fun', _, {function, Module, Name, Arity}}) ->
    io:format("remote_fun~n"), scalar(Module), scalar(Name), scalar(Arity);
scalar({'fun', _, {clauses, Clauses}}) ->
    io:format("fun\t-\t~B~n", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({named_fun, _, Name, Clauses}) ->
    io:format("fun\t~s\t~B~n", [hex(atom_to_list(Name)),length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'try', _, Body, Of, Catch, After}) ->
    io:format("try\t~B\t~B\t~B\t~B~n", [length(Body),length(Of),length(Catch),length(After)]),
    lists:foreach(fun scalar/1, Body), lists:foreach(fun clause/1, Of),
    lists:foreach(fun clause/1, Catch), lists:foreach(fun scalar/1, After);
scalar({'maybe', _, Body}) ->
    io:format("maybe\t~B\t0~n", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({'maybe', _, Body, {'else', _, Clauses}}) ->
    io:format("maybe\t~B\t~B~n", [length(Body),length(Clauses)]),
    lists:foreach(fun scalar/1, Body), lists:foreach(fun clause/1, Clauses);
scalar({maybe_match, _, Left, Right}) -> io:format("maybe_match~n"), scalar(Left), scalar(Right);
scalar({block, _, Body}) ->
    io:format("block\t~B~n", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({'case', _, Value, Clauses}) ->
    io:format("case\t~B~n", [length(Clauses)]), scalar(Value), lists:foreach(fun clause/1, Clauses);
scalar({'if', _, Clauses}) ->
    io:format("if\t~B~n", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'receive', _, Clauses}) ->
    io:format("receive\t~B\t0~n", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'receive', _, Clauses, Timeout, Body}) ->
    io:format("receive\t~B\t1~n", [length(Clauses)]), lists:foreach(fun clause/1, Clauses),
    scalar(Timeout), io:format("after\t~B~n", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({map, _, Base, Fields}) -> map({some, Base}, Fields);
scalar({record, _, Name, Fields}) -> record(none, Name, Fields);
scalar({record, _, Base, Name, Fields}) -> record({some, Base}, Name, Fields);
scalar({record_field, _, Base, Name, Field}) ->
    io:format("record_access~n"), scalar(Base), record_name(Name), scalar(Field);
scalar({record_index, _, Name, Field}) ->
    io:format("record_index~n"), scalar({atom, 0, Name}), scalar(Field);
scalar({op, _, Op, Arg}) -> io:format("unary\t~s~n", [Op]), scalar(Arg);
scalar({op, _, Op, Left, Right}) -> io:format("binary\t~s~n", [Op]), scalar(Left), scalar(Right);
scalar({match, _, Left, Right}) -> io:format("match~n"), scalar(Left), scalar(Right);
scalar({'catch', _, Expr}) -> io:format("catch~n"), scalar(Expr);
scalar({remote, _, Module, Function}) -> io:format("remote~n"), scalar(Module), scalar(Function);
scalar({call, _, Target, Arguments}) ->
    io:format("call\t~B~n", [length(Arguments)]), scalar(Target), lists:foreach(fun scalar/1, Arguments);
scalar({tuple, _, Elements}) ->
    io:format("tuple\t~B~n", [length(Elements)]), lists:foreach(fun scalar/1, Elements);
scalar({nil, _}) -> io:format("list\t0\t0~n");
scalar({cons, _, _, _} = List) ->
    {Elements, Tail} = spine(List),
    io:format("list\t~B\t~B~n", [length(Elements), case Tail of none -> 0; _ -> 1 end]),
    lists:foreach(fun scalar/1, Elements),
    case Tail of none -> ok; _ -> scalar(Tail) end;
scalar({bin, _, [{bin_element, _, {string, _, Value}, default, [utf8]}]}) -> field("binary_sigil", Value);
scalar({bin, _, Segments}) ->
    io:format("bitstring\t~B~n", [length(Segments)]), lists:foreach(fun segment/1, Segments);
scalar({var, _, Name}) -> field("var", atom_to_list(Name));
scalar({atom, _, Name}) -> field("atom", atom_to_list(Name));
scalar({integer, _, Value}) -> io:format("integer\t~B~n", [Value]);
scalar({float, _, Value}) -> io:format("float\t~s~n", [binary:encode_hex(<<Value:64/float>>, lowercase)]);
scalar({char, _, Value}) -> io:format("char\t~B~n", [Value]);
scalar({string, _, Value}) -> field("string", Value);
scalar(Other) -> erlang:error({unmapped_phase1_expression, Other}).

field(Kind, Value) -> io:format("~s\t~s~n", [Kind, hex(Value)]).
hex([]) -> "-";
hex(Value) -> binary:encode_hex(unicode:characters_to_binary(Value), lowercase).

%% Preserve explicit list tails when they are not another cons/nil spine.
spine({cons, _, H, T}) -> {Rest, Tail} = spine(T), {[H|Rest], Tail};
spine({nil, _}) -> {[], none};
spine(Other) -> {[], Other}.

clause({clause, _, Arguments, Guards, Body}) ->
    io:format("clause\t~B\t~B\t~B~n", [length(Arguments), length(Guards), length(Body)]),
    lists:foreach(fun scalar/1, Arguments),
    lists:foreach(fun(Tests) ->
        io:format("guard\t~B~n", [length(Tests)]), lists:foreach(fun scalar/1, Tests)
    end, Guards),
    lists:foreach(fun scalar/1, Body).

map(Base, Fields) ->
    io:format("map\t~B\t~B~n", [present(Base), length(Fields)]), optional(Base),
    lists:foreach(fun({Kind, _, Key, Value}) ->
        Op = case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end,
        io:format("map_field\t~s~n", [Op]), scalar(Key), scalar(Value)
    end, Fields).
record(Base, Name, Fields) ->
    io:format("record\t~B\t~B~n", [present(Base), length(Fields)]), optional(Base), record_name(Name),
    lists:foreach(fun({record_field, _, Key, Value}) ->
        io:format("record_field~n"), scalar(Key), scalar(Value)
    end, Fields).
present(none) -> 0;
present({some, _}) -> 1.
optional(none) -> ok;
optional({some, Expr}) -> scalar(Expr).
record_name([]) -> io:format("record_inferred~n");
record_name({Module, Name}) -> io:format("record_qualified\t~s\t~s~n", [hex(atom_to_list(Module)), hex(atom_to_list(Name))]);
record_name(Name) when is_atom(Name) -> field("record_local", atom_to_list(Name)).

segment({bin_element, _, Value, Size, Types}) ->
    Count = case Types of default -> "default"; _ -> integer_to_list(length(Types)) end,
    Explicit = case Size of default -> 0; _ -> 1 end,
    io:format("segment\t~B\t~s~n", [Explicit, Count]), scalar(Value),
    case Size of default -> ok; _ -> scalar(Size) end,
    case Types of default -> ok; _ -> lists:foreach(fun modifier/1, Types) end.
modifier({Name, Value}) -> io:format("modifier\t~s\t~B~n", [hex(atom_to_list(Name)), Value]);
modifier(Name) -> io:format("modifier\t~s\tnone~n", [hex(atom_to_list(Name))]).
