#!/usr/bin/env escript
%% Produce private normalized abstract forms, never a public stage format.
main([Mode, Input]) ->
    case list_to_integer(erlang:system_info(otp_release)) >= 29 of
        true -> run(Mode, Input), finish_records();
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
    node((length(Parameters)), "legacy_module name_hex=~s parameters=~B", [hex(atom_to_list(Name)), length(Parameters)]),
    lists:foreach(fun(V) -> scalar({var,0,V}) end, Parameters);
project({attribute, _, module, Name}) -> field("module", atom_to_list(Name));
project({attribute, _, file, {Name, Line}}) ->
    node(0, "file name_hex=~s line=~B", [hex(filename:basename(Name)), Line]);
project({attribute, _, export, Values}) -> node((length(Values)), "export functions=~B", [length(Values)]), arities(Values);
project({attribute, _, import, {Module, Values}}) -> node((length(Values)), "import module_hex=~s functions=~B", [hex(atom_to_list(Module)), length(Values)]), arities(Values);
project({attribute, _, import_record, {Module, Values}}) ->
    node((length(Values)), "import_record module_hex=~s names=~B", [hex(atom_to_list(Module)), length(Values)]),
    lists:foreach(fun term_value/1, Values);
project({attribute, _, Kind, {Name, Fields}}) when Kind =:= record; Kind =:= native_record ->
    node((length(Fields)), "record_decl name_hex=~s native=~B fields=~B", [hex(atom_to_list(Name)), bool(Kind =:= native_record), length(Fields)]),
    lists:foreach(fun declaration_field/1, Fields);
project({attribute, _, Kind, Value}) when Kind =:= doc; Kind =:= moduledoc ->
    node(1, "doc module=~B", [bool(Kind =:= moduledoc)]), documentation(Value);
project({attribute,_,Kind,{Name,Signatures}}) when Kind =:= spec; Kind =:= callback ->
    {Module,Function,Arity} = case Name of {F,A} -> {"-",F,A}; {M,F,A} -> {hex(atom_to_list(M)),F,A} end,
    node((length(Signatures)), "spec callback=~B module_hex=~s name_hex=~s arity=~B signatures=~B", [bool(Kind =:= callback), Module, hex(atom_to_list(Function)), Arity, length(Signatures)]),
    lists:foreach(fun specification_signature/1,Signatures);
project({attribute,_,Kind,{Name,Type,Parameters}}) when Kind =:= type; Kind =:= opaque; Kind =:= nominal ->
    Tag = case Kind of type -> 0; opaque -> 1; nominal -> 2 end,
    node(1+(length(Parameters)), "type_decl kind=~B name_hex=~s parameters=~B", [Tag, hex(atom_to_list(Name)), length(Parameters)]),
    lists:foreach(fun scalar/1,Parameters), type_value(Type);
project({attribute, _, Name, Value}) -> field("attribute", atom_to_list(Name)), term_value(Value);
project({function, _, Name, 0, [{clause, _, [], [], [Expr]}]}) ->
    field("function", atom_to_list(Name)), scalar(Expr);
project({function, _, Name, Arity, Clauses}) ->
    node((length(Clauses)), "function_full name_hex=~s arity=~B clauses=~B", [hex(atom_to_list(Name)), Arity, length(Clauses)]),
    lists:foreach(fun clause/1, Clauses);
project(Other) -> erlang:error({unmapped_phase1_form, Other}).

bool(true) -> 1;
bool(false) -> 0.
arities(Values) ->
    lists:foreach(fun({Name, Arity}) -> node(0, "arity name_hex=~s value=~B", [hex(atom_to_list(Name)), Arity]) end, Values).
declaration_field({typed_record_field,Field,Type}) -> node(2, "typed_field", []), type_value(Type), declaration_field(Field);
declaration_field({record_field,_,{atom,_,Name}}) -> node((0), "record_decl_field name_hex=~s default=0", [hex(atom_to_list(Name))]);
declaration_field({record_field,_,{atom,_,Name},Value}) ->
    node((1), "record_decl_field name_hex=~s default=1", [hex(atom_to_list(Name))]), scalar(Value).
documentation(Value) when is_map(Value) ->
    node(2*(map_size(Value)), "metadata entries=~B", [map_size(Value)]),
    lists:foreach(fun({K,V}) -> term_value(K), doc_value(K,V) end, lists:sort(maps:to_list(Value)));
documentation(Value) -> term_value(Value).
doc_value(equiv, {call,_,_,_}=Value) -> node(1, "equiv", []), scalar(Value);
doc_value(_, Value) -> term_value(Value).
term_value(Value) when is_atom(Value) -> scalar({atom,0,Value});
term_value(Value) when is_integer(Value) -> scalar({integer,0,Value});
term_value(Value) when is_float(Value) -> scalar({float,0,Value});
term_value(Value) when is_tuple(Value) ->
    node((tuple_size(Value)), "term_tuple elements=~B", [tuple_size(Value)]), lists:foreach(fun term_value/1, tuple_to_list(Value));
term_value([]) -> node(0, "nil", []);
term_value([H|T]) -> node(2, "cons", []), term_value(H), term_value(T);
term_value(Value) when is_map(Value) ->
    node(2*(map_size(Value)), "term_map entries=~B", [map_size(Value)]),
    lists:foreach(fun({K,V}) -> term_value(K), term_value(V) end, lists:sort(maps:to_list(Value)));
term_value(Value) when is_bitstring(Value) ->
    node(0, "term_bits bits=~s", [[ $0+B || <<B:1>> <= Value ]]);
term_value(Value) when is_function(Value) ->
    {module,M}=erlang:fun_info(Value,module), {name,N}=erlang:fun_info(Value,name), {arity,A}=erlang:fun_info(Value,arity),
    node(3, "term_fun", []), term_value(M), term_value(N), term_value(A).

type_value({ann_type,_,[V,T]}) -> node(2, "ann_type", []), scalar(V), type_value(T);
type_value({type,_,union,Types}) -> type_union(Types);
type_value({type,_,range,[A,B]}) -> node(2, "range", []), type_value(A), type_value(B);
type_value({type,_,tuple,any}) -> node(0, "tuple_any", []);
type_value({type,_,map,any}) -> node(0, "map_any", []);
type_value({type,_,binary,[A,B]}) -> node(2, "binary_type", []), type_value(A), type_value(B);
type_value({type,_,'fun',[]}) -> node(0, "fun_type arguments=unset", []);
type_value({type,_,'fun',[{type,_,any},R]}) -> node(1, "fun_type arguments=any", []), type_value(R);
type_value({type,_,'fun',[{type,_,product,Args},R]}) ->
    node(1+(length(Args)), "fun_type arguments=~B", [length(Args)]), lists:foreach(fun type_value/1,Args), type_value(R);
type_value({type,_,record,[Name|Fields]}) ->
    {M,N} = case Name of {atom,_,A} -> {"-",A}; {tuple,_,[{atom,_,Mod},{atom,_,A}]} -> {hex(atom_to_list(Mod)),A} end,
    node((length(Fields)), "record_type module_hex=~s name_hex=~s fields=~B", [M, hex(atom_to_list(N)), length(Fields)]), lists:foreach(fun type_value/1,Fields);
type_value({type,_,field_type,[{atom,_,Name},Type]}) -> field("type_field",atom_to_list(Name)), type_value(Type);
type_value({type,_,Kind,[K,V]}) when Kind =:= map_field_assoc; Kind =:= map_field_exact ->
    node(2, "type_field operator=~s", [case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end]), type_value(K), type_value(V);
type_value({remote_type,_,[{atom,_,M},{atom,_,N},Args]}) ->
    field("remote",atom_to_list(M)), type_value({user_type,0,N,Args});
type_value({Kind,_,Name,Args}) when Kind =:= type; Kind =:= user_type ->
    node((length(Args)), "~s name_hex=~s arguments=~B", [Kind, hex(atom_to_list(Name)), length(Args)]), lists:foreach(fun type_value/1,Args);
type_value({op,_,Op,A}) -> node(1, "unary operator=~s", [Op]), type_value(A);
type_value({op,_,Op,A,B}) -> node(2, "binary operator=~s", [Op]), type_value(A), type_value(B);
type_value(Value) -> scalar(Value).
type_union([T]) -> type_value(T);
type_union([T|Rest]) -> node(2, "union", []), type_value(T), type_union(Rest).

specification_signature({type,_,bounded_fun,[Function,Constraints]}) ->
    node(1+(length(Constraints)), "signature constraints=~B", [length(Constraints)]), type_value(Function), lists:foreach(fun constraint/1,Constraints);
specification_signature(Function) -> node(1+(0), "signature constraints=0", []), type_value(Function).
constraint({type,_,constraint,[{atom,_,is_subtype},[V,T]]}) -> node(2, "constraint", []), scalar(V), type_value(T).

scalar({map, _, Fields}) -> map(none, Fields);
scalar({lc, _, Templates, Qualifiers}) ->
    Values = templates(Templates),
    node((length(Values))+(length(Qualifiers)), "lc templates=~B qualifiers=~B", [length(Values), length(Qualifiers)]),
    lists:foreach(fun scalar/1, Values), lists:foreach(fun qualifier/1, Qualifiers);
scalar({mc, _, Templates, Qualifiers}) ->
    Values = templates(Templates),
    node((length(Values))+(length(Qualifiers)), "mc templates=~B qualifiers=~B", [length(Values), length(Qualifiers)]),
    lists:foreach(fun map_template/1, Values), lists:foreach(fun qualifier/1, Qualifiers);
scalar({bc, _, Template, Qualifiers}) ->
    node(1+(length(Qualifiers)), "bc qualifiers=~B", [length(Qualifiers)]), scalar(Template), lists:foreach(fun qualifier/1, Qualifiers);
scalar({'fun', _, {function, Name, Arity}}) ->
    node(2, "local_fun", []), scalar({atom,0,Name}), scalar({integer,0,Arity});
scalar({'fun', _, {function, Module, Name, Arity}}) ->
    node(3, "remote_fun", []), scalar(Module), scalar(Name), scalar(Arity);
scalar({'fun', _, {clauses, Clauses}}) ->
    node((length(Clauses)), "fun name_hex=- clauses=~B", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({named_fun, _, Name, Clauses}) ->
    node((length(Clauses)), "fun name_hex=~s clauses=~B", [hex(atom_to_list(Name)), length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'try', _, Body, Of, Catch, After}) ->
    node((length(Body))+(length(Of))+(length(Catch))+(length(After)), "try body=~B clauses=~B handlers=~B after=~B", [length(Body), length(Of), length(Catch), length(After)]),
    lists:foreach(fun scalar/1, Body), lists:foreach(fun clause/1, Of),
    lists:foreach(fun clause/1, Catch), lists:foreach(fun scalar/1, After);
scalar({'maybe', _, Body}) ->
    node((length(Body))+(0), "maybe body=~B clauses=0", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({'maybe', _, Body, {'else', _, Clauses}}) ->
    node((length(Body))+(length(Clauses)), "maybe body=~B clauses=~B", [length(Body), length(Clauses)]),
    lists:foreach(fun scalar/1, Body), lists:foreach(fun clause/1, Clauses);
scalar({maybe_match, _, Left, Right}) -> node(2, "maybe_match", []), scalar(Left), scalar(Right);
scalar({block, _, Body}) ->
    node((length(Body)), "block body=~B", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({'case', _, Value, Clauses}) ->
    node(1+(length(Clauses)), "case clauses=~B", [length(Clauses)]), scalar(Value), lists:foreach(fun clause/1, Clauses);
scalar({'if', _, Clauses}) ->
    node((length(Clauses)), "if clauses=~B", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'receive', _, Clauses}) ->
    node((length(Clauses))+2*(0), "receive clauses=~B timeout=0", [length(Clauses)]), lists:foreach(fun clause/1, Clauses);
scalar({'receive', _, Clauses, Timeout, Body}) ->
    node((length(Clauses))+2*(1), "receive clauses=~B timeout=1", [length(Clauses)]), lists:foreach(fun clause/1, Clauses),
    scalar(Timeout), node((length(Body)), "after body=~B", [length(Body)]), lists:foreach(fun scalar/1, Body);
scalar({map, _, Base, Fields}) -> map({some, Base}, Fields);
scalar({record, _, Name, Fields}) -> record(none, Name, Fields);
scalar({record, _, Base, Name, Fields}) -> record({some, Base}, Name, Fields);
scalar({record_field, _, Base, Name, Field}) ->
    node(3, "record_access", []), scalar(Base), record_name(Name), scalar(Field);
scalar({record_index, _, Name, Field}) ->
    node(2, "record_index", []), scalar({atom, 0, Name}), scalar(Field);
scalar({op, _, Op, Arg}) -> node(1, "unary operator=~s", [Op]), scalar(Arg);
scalar({op, _, Op, Left, Right}) -> node(2, "binary operator=~s", [Op]), scalar(Left), scalar(Right);
scalar({match, _, Left, Right}) -> node(2, "match", []), scalar(Left), scalar(Right);
scalar({'catch', _, Expr}) -> node(1, "catch", []), scalar(Expr);
scalar({remote, _, Module, Function}) -> node(2, "remote", []), scalar(Module), scalar(Function);
scalar({call, _, Target, Arguments}) ->
    node(1+(length(Arguments)), "call arguments=~B", [length(Arguments)]), scalar(Target), lists:foreach(fun scalar/1, Arguments);
scalar({tuple, _, Elements}) ->
    node((length(Elements)), "tuple elements=~B", [length(Elements)]), lists:foreach(fun scalar/1, Elements);
scalar({nil, _}) -> node((0)+(0), "list elements=0 tail=0", []);
scalar({cons, _, _, _} = List) ->
    {Elements, Tail} = spine(List),
    node((length(Elements))+(case Tail of none -> 0; _ -> 1 end), "list elements=~B tail=~B", [length(Elements), case Tail of none -> 0; _ -> 1 end]),
    lists:foreach(fun scalar/1, Elements),
    case Tail of none -> ok; _ -> scalar(Tail) end;
scalar({bin, _, [{bin_element, _, {string, _, Value}, default, [utf8]}]}) -> field("binary_sigil", Value);
scalar({bin, _, Segments}) ->
    node((length(Segments)), "bitstring segments=~B", [length(Segments)]), lists:foreach(fun segment/1, Segments);
scalar({var, _, Name}) -> field("var", atom_to_list(Name));
scalar({atom, _, Name}) -> field("atom", atom_to_list(Name));
scalar({integer, _, Value}) -> node(0, "integer value=~B", [Value]);
scalar({float, _, Value}) -> node(0, "float bits=~s", [binary:encode_hex(<<Value:64/float>>, lowercase)]);
scalar({char, _, Value}) -> node(0, "char value=~B", [Value]);
scalar({string, _, Value}) -> field("string", Value);
scalar(Other) -> erlang:error({unmapped_phase1_expression, Other}).

field(Kind, Value) ->
    {Children, Name} = field_shape(Kind),
    node(Children, "~s ~s=~s", [Kind, Name, hex(Value)]).

field_shape("attribute") -> {1, "name_hex"};
field_shape("function") -> {1, "name_hex"};
field_shape("remote") -> {1, "module_hex"};
field_shape("type_field") -> {1, "name_hex"};
field_shape("string") -> {0, "value_hex"};
field_shape("binary_sigil") -> {0, "value_hex"};
field_shape(_) -> {0, "name_hex"}.

templates(Value) when is_list(Value) -> Value;
templates(Value) -> [Value].
map_template({Kind, _, Key, Value}) ->
    Op = case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end,
    node(2, "map_field operator=~s", [Op]), scalar(Key), scalar(Value).
qualifier({zip, _, Qualifiers}) ->
    node((length(Qualifiers)), "zip qualifiers=~B", [length(Qualifiers)]), lists:foreach(fun qualifier/1, Qualifiers);
qualifier({Kind, _, {map_field_exact, _, Key, Value}, Input}) when Kind =:= m_generate; Kind =:= m_generate_strict ->
    Op = case Kind of m_generate -> "<-"; m_generate_strict -> "<:-" end,
    node(3, "m_generate operator=~s", [Op]), scalar(Key), scalar(Value), scalar(Input);
qualifier({Kind, _, Pattern, Input}) when Kind =:= generate; Kind =:= generate_strict ->
    Op = case Kind of generate -> "<-"; generate_strict -> "<:-" end,
    node(2, "generate operator=~s", [Op]), scalar(Pattern), scalar(Input);
qualifier({Kind, _, Pattern, Input}) when Kind =:= b_generate; Kind =:= b_generate_strict ->
    Op = case Kind of b_generate -> "<="; b_generate_strict -> "<:=" end,
    node(2, "b_generate operator=~s", [Op]), scalar(Pattern), scalar(Input);
qualifier(Expression) -> node(1, "filter", []), scalar(Expression).
hex([]) -> "-";
hex(Value) -> binary:encode_hex(unicode:characters_to_binary(Value), lowercase).

%% Preserve explicit list tails when they are not another cons/nil spine.
spine({cons, _, H, T}) -> {Rest, Tail} = spine(T), {[H|Rest], Tail};
spine({nil, _}) -> {[], none};
spine(Other) -> {[], Other}.

clause({clause, _, Arguments, Guards, Body}) ->
    node((length(Arguments))+(length(Guards))+(length(Body)), "clause arguments=~B guards=~B body=~B", [length(Arguments), length(Guards), length(Body)]),
    lists:foreach(fun scalar/1, Arguments),
    lists:foreach(fun(Tests) ->
        node((length(Tests)), "guard tests=~B", [length(Tests)]), lists:foreach(fun scalar/1, Tests)
    end, Guards),
    lists:foreach(fun scalar/1, Body).

map(Base, Fields) ->
    node((present(Base))+(length(Fields)), "map base=~B fields=~B", [present(Base), length(Fields)]), optional(Base),
    lists:foreach(fun({Kind, _, Key, Value}) ->
        Op = case Kind of map_field_assoc -> "=>"; map_field_exact -> ":=" end,
        node(2, "map_field operator=~s", [Op]), scalar(Key), scalar(Value)
    end, Fields).
record(Base, Name, Fields) ->
    node(1+(present(Base))+(length(Fields)), "record base=~B fields=~B", [present(Base), length(Fields)]), optional(Base), record_name(Name),
    lists:foreach(fun({record_field, _, Key, Value}) ->
        node(2, "record_field", []), scalar(Key), scalar(Value)
    end, Fields).
present(none) -> 0;
present({some, _}) -> 1.
optional(none) -> ok;
optional({some, Expr}) -> scalar(Expr).
record_name([]) -> node(0, "record_inferred", []);
record_name({Module, Name}) -> node(0, "record_qualified module_hex=~s name_hex=~s", [hex(atom_to_list(Module)), hex(atom_to_list(Name))]);
record_name(Name) when is_atom(Name) -> field("record_local", atom_to_list(Name)).

segment({bin_element, _, Value, Size, Types}) ->
    Count = case Types of default -> "default"; _ -> integer_to_list(length(Types)) end,
    Explicit = case Size of default -> 0; _ -> 1 end,
    node(1 + Explicit + case Types of default -> 0; _ -> length(Types) end, "segment size=~B modifiers=~s", [Explicit, Count]), scalar(Value),
    case Size of default -> ok; _ -> scalar(Size) end,
    case Types of default -> ok; _ -> lists:foreach(fun modifier/1, Types) end.
modifier({Name, Value}) -> node(0, "modifier name_hex=~s parameter=~B", [hex(atom_to_list(Name)), Value]);
modifier(Name) -> node(0, "modifier name_hex=~s parameter=none", [hex(atom_to_list(Name))]).

%% Child counts keep projection nesting independent of Erlang's traversal helpers.
node(Children, Format, Values) ->
    Stack = case get(ast_remaining) of undefined -> []; Existing -> Existing end,
    Parents = case Stack of [] -> []; [N|Rest] -> [N-1|Rest] end,
    io:put_chars([lists:duplicate(2*length(Stack), $ ), $(, io_lib:format(Format, Values)]),
    case Children of
        0 -> io:put_chars(")\n"), close_records(Parents);
        _ -> io:put_chars("\n"), put(ast_remaining, [Children|Parents])
    end.

close_records([0|Rest]) ->
    io:put_chars([lists:duplicate(2*length(Rest), $ ), ")\n"]), close_records(Rest);
close_records(Stack) -> put(ast_remaining, Stack).

finish_records() ->
    case get(ast_remaining) of
        undefined -> ok;
        [] -> ok;
        Stack -> erlang:error({unfinished_ast_record, Stack})
    end.
