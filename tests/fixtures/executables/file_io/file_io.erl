-module(file_io).
-export([main/1]).

% Files and standard input through ports: whole-file reads and writes, open files in list and binary mode with
% reads, lines, positions and appends, path operations and their errors, and io:get_line/get_chars on standard
% input. Files are created in the working directory and removed again.

show(Label, Value) -> io:format("~s ~p~n", [Label, Value]).

files() ->
    [file:delete(Name) || Name <- ["t1.txt", "t2.txt", "t3.txt"]],
    show("write_file", file:write_file("t1.txt", ["abc\n", <<"def\nghi">>])),
    show("read_file", file:read_file("t1.txt")),
    show("read_file missing", file:read_file("missing.txt")),
    {ok, F} = file:open("t1.txt", [read]),
    show("is_pid", is_pid(F)),
    show("read", file:read(F, 2)),
    show("read_line", [file:read_line(F) || _ <- lists:seq(1, 4)]),
    show("read at eof", file:read(F, 5)),
    show("position", [file:position(F, P) || P <- [bof, 4, {cur, -1}, eof, {bof, -5}]]),
    {ok, 4} = file:position(F, 4),
    show("read after position", file:read(F, 3)),
    show("get_line of a file", io:get_line(F, "")),
    show("close", file:close(F)),
    show("read closed", file:read(F, 1)),
    show("close again", file:close(F)),
    {ok, W} = file:open("t2.txt", [write, binary]),
    show("write", file:write(W, [<<"one">>, $\n, "two\n"])),
    show("read write-only", file:read(W, 1)),
    file:close(W),
    {ok, A} = file:open("t2.txt", [append]),
    file:write(A, "three\n"),
    file:close(A),
    show("appended", file:read_file("t2.txt")),
    {ok, R} = file:open("t2.txt", [read, binary]),
    show("binary read", [file:read(R, 3), file:read_line(R)]),
    file:close(R),
    show("open errors", [
        file:open("missing.txt", [read]),
        file:open("t2.txt", [write, exclusive]),
        file:open(".", [write]),
        file:open(1, [read])
    ]),
    show("rename", [file:rename("t2.txt", "t3.txt"), file:rename("t2.txt", "t4.txt")]),
    show("delete", [file:delete("t3.txt"), file:delete("t3.txt")]),
    show("make_dir", [file:make_dir("tdir"), file:make_dir("tdir")]),
    file:write_file("tdir/b", "b"),
    file:write_file("tdir/a", "a"),
    {ok, Names} = file:list_dir("tdir"),
    show("list_dir", [lists:sort(Names), file:list_dir("no_such_dir")]),
    [file:delete(Name) || Name <- ["tdir/a", "tdir/b"]],
    show("del_dir", file:del_dir("tdir")),
    show("read_file directory", file:read_file(".")),
    show("write_file bad data", file:write_file("t1.txt", [a])),
    file:delete("t1.txt").

input() ->
    show("get_line", io:get_line("prompt> ")),
    show("get_line", io:get_line("")),
    show("get_chars", io:get_chars("", 3)),
    show("get_line", io:get_line("")),
    show("get_line at the end", io:get_line("")),
    show("get_chars at eof", io:get_chars("", 2)).

main(["files"]) -> files();
main(["input"]) -> input().
