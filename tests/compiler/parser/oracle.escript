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
run("builder_exception", Input) ->
    {ok,File}=file:open(Input,[read,{encoding,utf8}]),
    {ok,Tokens,_}=io:scan_erl_form(File,'',{1,1}), file:close(File),
    Result = try erl_parse:parse_form(Tokens) of
        Value -> {unexpected_builder_result,Value}
    catch error:function_clause -> builder_exception;
          error:{badmatch,_} -> builder_exception
    end,
    case Result of builder_exception -> io:format("builder_exception~n"); _ -> erlang:error(Result) end;
run("epp", Input) -> with_epp(Input, fun expanded/1);
run("accept", Input) ->
    {ok, Forms} = epp:parse_file(Input, [], []),
    Failed = lists:any(fun({error, _}) -> true; (_) -> false end, Forms),
    io:format("~s~n", [case Failed of true -> "rejected"; false -> "accepted" end]);
run("phase1", Input) -> with_epp(Input, fun phase1/1);
run("lint", Input) ->
    {ok, Forms, Extra} = epp:parse_file(Input, [extra]),
    %% Match compile.erl: lint needs the feature state retained by preprocessing.
    Options = [{features, proplists:get_value(features, Extra, [])}],
    Result = case erl_lint:module(Forms, Input, Options) of
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

project({attribute, _, module, {Name, Parameters}}) ->
    io:format("legacy_module\t~s\t~B~n", [hex(atom_to_list(Name)),length(Parameters)]),
    lists:foreach(fun(V) -> scalar({var,0,V}) end, Parameters);
project({attribute, _, module, Name}) -> field("module", atom_to_list(Name));
project({attribute, _, file, {Name, Line}}) ->
    io:format("file\t~s\t~B~n", [hex(filename:basename(Name)), Line]);
project({attribute, _, export, Values}) -> io:format("export\t"), arities(Values);
project({attribute, _, import, {Module, Values}}) -> io:format("import\t~s\t", [hex(atom_to_list(Module))]), arities(Values);
project({attribute, _, import_record, {Module, Values}}) ->
    io:format("import_record\t~s\t~B~n", [hex(atom_to_list(Module)),length(Values)]),
    lists:foreach(fun term_value/1, Values);
project({attribute, _, Kind, {Name, Fields}}) when Kind =:= record; Kind =:= native_record ->
    io:format("record_decl\t~s\t~B\t~B~n", [hex(atom_to_list(Name)), bool(Kind =:= native_record),length(Fields)]),
    lists:foreach(fun declaration_field/1, Fields);
project({attribute, _, Kind, Value}) when Kind =:= doc; Kind =:= moduledoc ->
    io:format("doc\t~B~n", [bool(Kind =:= moduledoc)]), documentation(Value);
project({attribute,_,Kind,{Name,Signatures}}) when Kind =:= spec; Kind =:= callback ->
    {Module,Function,Arity} = case Name of {F,A} -> {"-",F,A}; {M,F,A} -> {hex(atom_to_list(M)),F,A} end,
    io:format("spec\t~B\t~s\t~s\t~B\t~B~n",[bool(Kind =:= callback),Module,hex(atom_to_list(Function)),Arity,length(Signatures)]),
    lists:foreach(fun specification_signature/1,Signatures);
project({attribute,_,Kind,{Name,Type,Parameters}}) when Kind =:= type; Kind =:= opaque; Kind =:= nominal ->
    Tag = case Kind of type -> 0; opaque -> 1; nominal -> 2 end,
    io:format("type_decl\t~B\t~s\t~B~n",[Tag,hex(atom_to_list(Name)),length(Parameters)]),
    lists:foreach(fun scalar/1,Parameters), type_value(Type);
project({attribute, _, Name, Value}) -> field("attribute", atom_to_list(Name)), term_value(Value);
project({function, _, Name, 0, [{clause, _, [], [], [Expr]}]}) ->
    field("function", atom_to_list(Name)), scalar(Expr);
project({function, _, Name, Arity, Clauses}) ->
    io:format("function_full\t~s\t~B\t~B~n", [hex(atom_to_list(Name)), Arity, length(Clauses)]),
    lists:foreach(fun clause/1, Clauses);
project(Other) -> erlang:error({unmapped_phase1_form, Other}).

bool(true) -> 1;
bool(false) -> 0.
arities(Values) ->
    io:format("~B~n", [length(Values)]),
    lists:foreach(fun({Name, Arity}) -> io:format("~s\t~B~n", [hex(atom_to_list(Name)),Arity]) end, Values).
declaration_field({typed_record_field,Field,Type}) -> io:format("typed_field~n"), type_value(Type), declaration_field(Field);
declaration_field({record_field,_,{atom,_,Name}}) -> io:format("record_decl_field\t~s\t0~n", [hex(atom_to_list(Name))]);
declaration_field({record_field,_,{atom,_,Name},Value}) ->
    io:format("record_decl_field\t~s\t1~n", [hex(atom_to_list(Name))]), scalar(Value).
documentation(Value) when is_map(Value) ->
    io:format("metadata\t~B~n", [map_size(Value)]),
    lists:foreach(fun({K,V}) -> term_value(K), doc_value(K,V) end, lists:sort(maps:to_list(Value)));
documentation(Value) -> term_value(Value).
doc_value(equiv, {call,_,_,_}=Value) -> io:format("equiv~n"), scalar(Value);
doc_value(_, Value) -> term_value(Value).
term_value(Value) when is_atom(Value) -> scalar({atom,0,Value});
term_value(Value) when is_integer(Value) -> scalar({integer,0,Value});
term_value(Value) when is_float(Value) -> scalar({float,0,Value});
term_value(Value) when is_tuple(Value) ->
    io:format("term_tuple\t~B~n", [tuple_size(Value)]), lists:foreach(fun term_value/1, tuple_to_list(Value));
term_value([]) -> io:format("nil~n");
term_value([H|T]) -> io:format("cons~n"), term_value(H), term_value(T);
term_value(Value) when is_map(Value) ->
    io:format("term_map\t~B~n", [map_size(Value)]),
    lists:foreach(fun({K,V}) -> term_value(K), term_value(V) end, lists:sort(maps:to_list(Value)));
term_value(Value) when is_bitstring(Value) ->
    io:format("term_bits\t~s~n", [[ $0+B || <<B:1>> <= Value ]]);
term_value(Value) when is_function(Value) ->
    {module,M}=erlang:fun_info(Value,module), {name,N}=erlang:fun_info(Value,name), {arity,A}=erlang:fun_info(Value,arity),
    io:format("term_fun~n"), term_value(M), term_value(N), term_value(A).

type_value({ann_type,_,[V,T]}) -> io:format("ann_type~n"), scalar(V), type_value(T);
type_value({type,_,union,Types}) -> type_union(Types);
type_value({type,_,range,[A,B]}) -> io:format("range~n"), type_value(A), type_value(B);
type_value({type,_,tuple,any}) -> io:format("tuple_any~n");
type_value({type,_,map,any}) -> io:format("map_any~n");
type_value({type,_,binary,[A,B]}) -> io:format("binary_type~n"), type_value(A), type_value(B);
type_value({type,_,'fun',[]}) -> io:format("fun_type\tunset~n");
type_value({type,_,'fun',[{type,_,any},R]}) -> io:format("fun_type\tany~n"), type_value(R);
type_value({type,_,'fun',[{type,_,product,Args},R]}) ->
    io:format("fun_type\t~B~n",[length(Args)]), lists:foreach(fun type_value/1,Args), type_value(R);
type_value({type,_,record,[Name|Fields]}) ->
    {M,N} = case Name of {atom,_,A} -> {"-",A}; {tuple,_,[{atom,_,Mod},{atom,_,A}]} -> {hex(atom_to_list(Mod)),A} end,
    io:format("record_type\t~s\t~s\t~B~n",[M,hex(atom_to_list(N)),length(Fields)]), lists:foreach(fun type_value/1,Fields);
type_value({type,_,field_type,[{atom,_,Name},Type]}) -> field("type_field",atom_to_list(Name)), type_value(Type);
type_value({type,_,Kind,[K,V]}) when Kind =:= map_field_assoc; Kind =:= map_field_exact ->
    io:format("type_field\t~s~n",[case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end]), type_value(K), type_value(V);
type_value({remote_type,_,[{atom,_,M},{atom,_,N},Args]}) ->
    field("remote",atom_to_list(M)), type_value({user_type,0,N,Args});
type_value({Kind,_,Name,Args}) when Kind =:= type; Kind =:= user_type ->
    io:format("~s\t~s\t~B~n",[Kind,hex(atom_to_list(Name)),length(Args)]), lists:foreach(fun type_value/1,Args);
type_value({op,_,Op,A}) -> io:format("unary\t~s~n",[Op]), type_value(A);
type_value({op,_,Op,A,B}) -> io:format("binary\t~s~n",[Op]), type_value(A), type_value(B);
type_value(Value) -> scalar(Value).
type_union([T]) -> type_value(T);
type_union([T|Rest]) -> io:format("union~n"), type_value(T), type_union(Rest).

specification_signature({type,_,bounded_fun,[Function,Constraints]}) ->
    io:format("signature\t~B~n",[length(Constraints)]), type_value(Function), lists:foreach(fun constraint/1,Constraints);
specification_signature(Function) -> io:format("signature\t0~n"), type_value(Function).
constraint({type,_,constraint,[{atom,_,is_subtype},[V,T]]}) -> io:format("constraint~n"), scalar(V), type_value(T).

scalar({map, _, Fields}) -> map(none, Fields);
scalar({lc, _, Templates, Qualifiers}) ->
    Values = templates(Templates),
    io:format("lc\t~B\t~B~n", [length(Values),length(Qualifiers)]),
    lists:foreach(fun scalar/1, Values), lists:foreach(fun qualifier/1, Qualifiers);
scalar({mc, _, Templates, Qualifiers}) ->
    Values = templates(Templates),
    io:format("mc\t~B\t~B~n", [length(Values),length(Qualifiers)]),
    lists:foreach(fun map_template/1, Values), lists:foreach(fun qualifier/1, Qualifiers);
scalar({bc, _, Template, Qualifiers}) ->
    io:format("bc\t~B~n", [length(Qualifiers)]), scalar(Template), lists:foreach(fun qualifier/1, Qualifiers);
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

templates(Value) when is_list(Value) -> Value;
templates(Value) -> [Value].
map_template({Kind, _, Key, Value}) ->
    Op = case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end,
    io:format("map_field\t~s~n", [Op]), scalar(Key), scalar(Value).
qualifier({zip, _, Qualifiers}) ->
    io:format("zip\t~B~n", [length(Qualifiers)]), lists:foreach(fun qualifier/1, Qualifiers);
qualifier({Kind, _, {map_field_exact, _, Key, Value}, Input}) when Kind =:= m_generate; Kind =:= m_generate_strict ->
    Op = case Kind of m_generate -> "<-"; m_generate_strict -> "<:-" end,
    io:format("m_generate\t~s~n", [Op]), scalar(Key), scalar(Value), scalar(Input);
qualifier({Kind, _, Pattern, Input}) when Kind =:= generate; Kind =:= generate_strict ->
    Op = case Kind of generate -> "<-"; generate_strict -> "<:-" end,
    io:format("generate\t~s~n", [Op]), scalar(Pattern), scalar(Input);
qualifier({Kind, _, Pattern, Input}) when Kind =:= b_generate; Kind =:= b_generate_strict ->
    Op = case Kind of b_generate -> "<="; b_generate_strict -> "<:=" end,
    io:format("b_generate\t~s~n", [Op]), scalar(Pattern), scalar(Input);
qualifier(Expression) -> io:format("filter~n"), scalar(Expression).
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
