%% Base and built-in types of the Erlang type language (https://www.erlang.org/doc/system/typespec.html): each
%% function's -spec names one (checking that declarations resolve) and its body produces a value of it; `expect:` is
%% what inference should find (docs/semantic.md#inference-expectations). Categories print by their built-in names
%% (boolean(), binary(), non_neg_integer(), ...), bounded integer sets as ranges. Ports have no values yet (plan
%% step 53), so port() and identifier() appear in declarations only.
-module(base_types).
-export([
    any_value/1,
    no_return_value/0,
    dynamic_value/1,
    pid_value/0,
    spawned_pid/0,
    reference_value/0,
    nil_value/0,
    atom_value/1,
    singleton_atom/0,
    empty_bitstring/0,
    sized_bitstring/0,
    binary_value/1,
    comprehended_binary/1,
    nonempty_binary_value/1,
    bitstring_value/1,
    float_value/1,
    divided/1,
    fun_value/0,
    remote_fun/0,
    integer_value/1,
    integer_range/1,
    byte_value/1,
    char_value/0,
    arity_value/1,
    non_neg_integer_value/1,
    pos_integer_value/1,
    neg_integer_value/1,
    length_value/1,
    byte_size_value/1,
    number_value/1,
    boolean_test/1,
    boolean_comparison/1,
    list_value/1,
    nonempty_list_value/1,
    improper_list/0,
    string_value/1,
    nonempty_string_value/1,
    iolist_value/0,
    empty_map/0,
    map_value/1,
    empty_tuple/0,
    tuple_value/1,
    mfa_value/0,
    module_value/0,
    timeout_value/1,
    identifier_value/1
]).
-export_type([declared/0]).

%% Every built-in type resolves in declarations, including those without values here.
-type declared() ::
    port()
    | identifier()
    | node()
    | iodata()
    | nonempty_bitstring()
    | maybe_improper_list()
    | nonempty_maybe_improper_list(integer(), atom())
    | nil()
    | term().

%% expect: any_value(term()) -> argument 1
-spec any_value(any()) -> any().
any_value(X) -> X.

%% expect: no_return_value() -> none()
-spec no_return_value() -> no_return().
no_return_value() -> erlang:error(stop).

%% expect: dynamic_value(term()) -> argument 1
-spec dynamic_value(dynamic()) -> dynamic().
dynamic_value(X) -> X.

%% Processes and references.

%% expect: pid_value() -> pid()
-spec pid_value() -> pid().
pid_value() -> self().

%% expect: spawned_pid() -> pid()
-spec spawned_pid() -> pid().
spawned_pid() -> spawn(fun() -> ok end).

%% expect: reference_value() -> reference()
-spec reference_value() -> reference().
reference_value() -> make_ref().

%% expect: identifier_value(term()) -> reference() | pid()
-spec identifier_value(term()) -> identifier().
identifier_value(X) ->
    case X of
        pid -> self();
        _ -> make_ref()
    end.

%% Atoms.

%% expect: nil_value() -> []
-spec nil_value() -> nil().
nil_value() -> [].

%% expect: atom_value(string()) -> atom()
-spec atom_value(string()) -> atom().
atom_value(Name) -> list_to_atom(Name).

%% expect: singleton_atom() -> ok
-spec singleton_atom() -> ok.
singleton_atom() -> ok.

%% expect: module_value() -> base_types
-spec module_value() -> module().
module_value() -> ?MODULE.

%% Bitstrings and binaries.

%% expect: empty_bitstring() -> <<>>
-spec empty_bitstring() -> <<>>.
empty_bitstring() -> <<>>.

%% expect: sized_bitstring() -> <<_:3>>
-spec sized_bitstring() -> <<_:3>>.
sized_bitstring() -> <<5:3>>.

%% expect: binary_value(term()) -> binary()
-spec binary_value(iolist()) -> binary().
binary_value(Data) -> list_to_binary(Data).

%% expect: comprehended_binary(term()) -> binary()
-spec comprehended_binary([byte()]) -> binary().
comprehended_binary(Bytes) -> <<<<B>> || B <- Bytes>>.

%% expect: nonempty_binary_value(binary()) -> nonempty_binary()
-spec nonempty_binary_value(binary()) -> nonempty_binary().
nonempty_binary_value(Rest) -> <<1, Rest/binary>>.

%% expect: bitstring_value(bitstring()) -> nonempty_bitstring()
-spec bitstring_value(bitstring()) -> bitstring().
bitstring_value(Rest) -> <<1:1, Rest/bitstring>>.

%% Floats, integers and numbers.

%% expect: float_value(number()) -> float()
-spec float_value(number()) -> float().
float_value(X) -> float(X).

%% expect: divided(number()) -> float()
-spec divided(number()) -> float().
divided(X) -> X / 2.

%% expect: integer_value(number()) -> integer()
-spec integer_value(number()) -> integer().
integer_value(X) -> trunc(X).

%% expect: integer_range(1..10) -> 1..10
-spec integer_range(integer()) -> 1..10.
integer_range(X) when is_integer(X), X >= 1, X =< 10 -> X.

%% expect: byte_value(integer()) -> 0..255
-spec byte_value(integer()) -> byte().
byte_value(X) -> X band 255.

%% expect: char_value() -> 97
-spec char_value() -> char().
char_value() -> $a.

%% expect: arity_value(0..255) -> 0..255
-spec arity_value(integer()) -> arity().
arity_value(X) when is_integer(X), X >= 0, X =< 255 -> X.

%% expect: non_neg_integer_value(integer()) -> non_neg_integer()
-spec non_neg_integer_value(integer()) -> non_neg_integer().
non_neg_integer_value(X) when is_integer(X) -> abs(X).

%% expect: pos_integer_value(pos_integer()) -> pos_integer()
-spec pos_integer_value(integer()) -> pos_integer().
pos_integer_value(X) when is_integer(X), X > 0 -> X.

%% expect: neg_integer_value(neg_integer()) -> neg_integer()
-spec neg_integer_value(integer()) -> neg_integer().
neg_integer_value(X) when is_integer(X), X < 0 -> X.

%% expect: length_value(list()) -> non_neg_integer()
-spec length_value(list()) -> non_neg_integer().
length_value(List) -> length(List).

%% expect: byte_size_value(bitstring()) -> non_neg_integer()
-spec byte_size_value(binary()) -> non_neg_integer().
byte_size_value(Binary) -> byte_size(Binary).

%% expect: number_value(number()) -> number()
-spec number_value(number()) -> number().
number_value(X) when is_number(X) -> X * 2.

%% Booleans.

%% expect: boolean_test(term()) -> boolean()
-spec boolean_test(term()) -> boolean().
boolean_test(X) -> is_atom(X).

%% expect: boolean_comparison(term()) -> boolean()
-spec boolean_comparison(term()) -> boolean().
boolean_comparison(X) -> X > 1.

%% Lists and strings.

%% expect: list_value(tuple()) -> list()
-spec list_value(tuple()) -> list().
list_value(Tuple) -> tuple_to_list(Tuple).

%% expect: nonempty_list_value(term()) -> [term(), ...]
-spec nonempty_list_value(term()) -> nonempty_list().
nonempty_list_value(X) -> [X].

%% expect: improper_list() -> nonempty_improper_list(1, a)
-spec improper_list() -> nonempty_improper_list(integer(), atom()).
improper_list() -> [1 | a].

%% expect: string_value(atom()) -> string()
-spec string_value(atom()) -> string().
string_value(Atom) -> atom_to_list(Atom).

%% expect: nonempty_string_value(integer()) -> nonempty_string()
-spec nonempty_string_value(integer()) -> nonempty_string().
nonempty_string_value(Integer) -> integer_to_list(Integer).

%% expect: iolist_value() -> [[98, ...] | <<_:8>>, ...]
-spec iolist_value() -> iolist().
iolist_value() -> [<<"a">>, "b"].

%% Maps and tuples.

%% expect: empty_map() -> #{}
-spec empty_map() -> #{}.
empty_map() -> #{}.

%% expect: map_value(map()) -> map()
-spec map_value(map()) -> map().
map_value(Map) -> Map#{key => value}.

%% expect: empty_tuple() -> {}
-spec empty_tuple() -> {}.
empty_tuple() -> {}.

%% expect: tuple_value(list()) -> tuple()
-spec tuple_value(list()) -> tuple().
tuple_value(List) -> list_to_tuple(List).

%% expect: mfa_value() -> {lists, reverse, 1}
-spec mfa_value() -> mfa().
mfa_value() -> {lists, reverse, 1}.

%% Funs.

%% expect: fun_value() -> fun(() -> ok)
-spec fun_value() -> fun(() -> ok).
fun_value() -> fun() -> ok end.

%% expect: remote_fun() -> fun((term()) -> term())
-spec remote_fun() -> function().
remote_fun() -> fun lists:reverse/1.

%% Unions of categories.

%% expect: timeout_value(non_neg_integer() | forever) -> non_neg_integer() | infinity
-spec timeout_value(term()) -> timeout().
timeout_value(X) ->
    case X of
        forever -> infinity;
        N when is_integer(N), N >= 0 -> N
    end.
