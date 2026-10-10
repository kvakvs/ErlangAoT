%% module_info/0,1, which the compiler adds to every module, called locally, remotely, through funs and apply/3.
-module(module_info).
-export([main/1]).

main(_) ->
    show(info_target:module_info(module)),
    show(info_target:module_info(exports)),
    show(info_target:module_info(attributes)),
    show(info_target:module_info(functions)),
    show([info_target:module_info(nifs), info_target:module_info(native)]),
    show([Key || {Key, _} <- info_target:module_info()]),
    show(info_shape:module_info(exports)),
    show(apply(info_plain, module_info, [exports])),
    Info = fun info_plain:module_info/1,
    show(Info(module)),
    show(module_info(module)),
    Local = fun module_info/1,
    show(Local(exports)),
    show(vsn(info_plain:module_info(attributes))),
    show(digest(info_target:module_info(md5))),
    show(compile(info_target:module_info(compile))),
    show(
        try info_target:module_info(bogus) of
            Value -> Value
        catch
            Class:Reason -> {Class, Reason}
        end
    ).

show(Term) -> io:format("~p~n", [Term]).

%% Values the compiler derives itself are checked by shape only.
vsn([{vsn, [Version]}]) when is_integer(Version), Version >= 0 -> derived_vsn.

digest(Digest) when is_binary(Digest), byte_size(Digest) =:= 16 -> md5.

compile([{version, Version}, {options, []}, {source, Source}]) when is_list(Version) ->
    "lre.tegrat_ofni" ++ _ = lists:reverse(Source),
    compile.
