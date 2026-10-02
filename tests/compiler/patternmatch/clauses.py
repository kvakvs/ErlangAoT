"""Execute source-ordered clauses, complete OTP guard helpers and candidate isolation."""
import itertools
import json
import pathlib
import re
import sys
from stored import load
from evidence import digest, provenance, run
from immediate import native
from services import VALUES, write_calls
from bindings import compile_case


def kernels(otp, work):
    """Keep full guard helpers and label the immediate-only match-suite adaptation."""
    path = otp / "lib/compiler/test/guard_SUITE.erl"
    text = path.read_text(encoding="utf-8")
    definitions, records = [], []
    for name, arity in [("bool", 1)] + [("csemi4_orelse_" + ch, 4) for ch in "abcd"]:
        clause = re.search(r"^" + name + r"\(.*?^" + name + r"\([^\n]*? -> error\.", text, re.M | re.S).group()
        definitions.append((name, arity, clause))
        records.append({"function": f"{name}/{arity}", "adaptations": "none; complete helper", "source": clause})
    matches = otp / "lib/compiler/test/match_SUITE.erl"
    match_text = matches.read_text(encoding="utf-8")
    char_alias = re.search(r"^char_alias_1\(.*?^char_alias_1\(_\) -> error\.", match_text, re.M | re.S).group()
    definitions.append(("char_alias_1", 1, char_alias.replace("char_alias_1(3.0=3.0) -> ok;\n", "")))
    records.append({"function": "char_alias_1/1", "adaptations": "float clause omitted until step 14; integer aliases and fallback unchanged", "source": char_alias})
    definitions += [
        ("overlap", 2, "overlap(X,X) -> repeated; overlap(0,_) -> zero; overlap(_,X) -> X."),
        ("rollback", 2, "rollback(X,Y) when X =:= Y -> X; rollback(Y,X) -> X."),
        ("failed_head", 2, "failed_head(X,X) -> same; failed_head(_,X) -> X."),
        ("alias", 2, "alias(X=Y,Z) when Y =:= Z -> X; alias(_,X) -> X."),
        ("errors", 1, "errors(X) when hd(X) -> impossible; errors(X) when is_atom(X) -> X; errors(_) -> fallback."),
        ("exhaust", 1, "exhaust(0) -> zero; exhaust(X) when X =:= ok -> X."),
        ("body_failure", 1, "body_failure(X) when is_integer(X) -> hd([]); body_failure(_) -> unreachable."),
        ("local", 1, "local(0) -> first; local(X) -> later(X)."),
        ("later", 1, "later(X) -> X."),
        ("common", 1, "common(0=X) -> X; common(X) -> X."),
        ("join", 1, "join(0) -> 1; join(_) -> 2."),
        ("wrong_spec", 2, "-spec wrong_spec(integer(),integer()) -> integer().\nwrong_spec(X,X) -> X; wrong_spec(_,X) -> X."),
        ("wide", 1, "; ".join(f"wide({i}) -> {i}" for i in range(128)) + "; wide(X) -> X.")]
    exports = ",".join(f"{name}/{arity}" for name, arity, _ in definitions)
    (work / "answer.erl").write_text(text.split("-module(")[0] + f"-module(answer).\n-export([{exports}]).\n" +
        "\n".join(body for _, _, body in definitions) + "\n", encoding="utf-8")
    (work / "client.erl").write_text("-module(client).\n-export([nested/1,exhaust/1,body_failure/1,repeat/2]).\n"
        "nested(0) -> first; nested(X) -> answer:local(X).\nexhaust(X) -> answer:exhaust(X).\n"
        "body_failure(X) -> answer:body_failure(X).\nrepeat(X,Y) -> answer:overlap(X,Y).\n", encoding="utf-8")
    calls = [("answer", name, [value]) for name, arity, _ in definitions if arity == 1 for value in VALUES]
    calls += [("answer", name, list(args)) for name, arity, _ in definitions if arity == 2
              for args in itertools.product(VALUES, repeat=2)]
    calls += [("answer", "char_alias_1", [value]) for value in [118,119,42]]
    calls += [("answer", "csemi4_orelse_" + ch, list(args)) for ch in "abcd"
              for args in itertools.product(["nil","tuple","ok"], [0,2], ["nil","tuple","ok"], [0,2])]
    calls += [("client", name, [value]) for name in ["nested","exhaust","body_failure"] for value in VALUES]
    calls += [("client", "repeat", list(args)) for args in itertools.product(VALUES, repeat=2)]
    calls += [("answer", "wide", [value]) for value in [0,63,127,128]]
    write_calls(work, calls)
    return {"calls": len(calls), "sources": {str(p.relative_to(otp)): digest(p) for p in [path,matches]},
            "helpers": records, "declarations": exports, "wrapper_sha256": digest(work / "answer.erl"),
            "client_sha256": digest(work / "client.erl")}


def rejection(tool, work):
    """Later or unused bodies still resolve calls, check scopes and reject unsupported syntax."""
    directory = work / "negative"
    (directory / "out").mkdir(parents=True, exist_ok=True)
    for file in (directory / "out").iterdir():
        if file.is_file():
            file.unlink()
    (directory / "out/sentinel").write_bytes(b"preserve")
    (directory / "client.erl").write_bytes(b"-module(client). -export([id/1]). id(X) -> X.\n")
    cases = [("later_heap", "f(X) -> X; f(_) -> [X || X <- []].", "heap expressions"),
             ("unused", "f(X) -> X. unused(X) -> X; unused(_) -> [X || X <- []].", "heap expressions"),
             ("later_guard", "f(X) -> X; f(X) when self() =:= X -> X.", "guards"),
             ("later_call", "f(X) -> X; f(_) -> missing().", "undefined function"),
             ("later_remote", "f(X) -> X; f(_) -> absent:f().", "unknown module"),
             ("later_cycle", "f(0) -> 0; f(X) -> g(X). g(X) -> f(X).", "recursive calls"),
             ("later_leak", "f(X,_) -> X; f(_,_) -> X.", "unbound variable X")]
    for name, body, diagnostic in cases:
        (directory / (name + ".erl")).write_text(f"-module({name}).\n\n{body}\n", encoding="utf-8")
        row = {"name": name, "diagnostic": diagnostic, "capability": ""}
        for options in [["-O0"], ["-O2"], ["-O0", "--no-type-specialization"], ["-O2", "--no-type-specialization"]]:
            for project in [False, True]:
                compile_case(tool, directory, row, options, project)
    run([tool, "--emit", "obj", "--artifact-dir", str(directory / "recovery"), str(directory / "client.erl")])
    return len(cases)


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "clauses", work)
    native(tool, cmake, source, work, settings, config, suffix)
    records["negative_cases"] = rejection(tool, work)
    for mode in ["--print-ir", "--print-optimized-ir", "--print-types"]:
        options = [] if mode == "--print-types" else ["-O2"]
        result = run([tool, *options, mode, str(work / "answer.erl"), str(work / "client.erl")])
        if mode == "--print-types":
            assert "binding=clause[1]" in result and "binding=clause[128]" in result
            summaries = {match.group(1): match.group(2) for match in
                         re.finditer(r'function "([^"]+)"[^\n]* result=([^\n]+)', result)}
            assert summaries["common"].endswith("[argument[0] relation]"), summaries["common"]
            assert "relation" not in summaries["rollback"], summaries["rollback"]
            assert summaries["join"] == "union(1, 2)", summaries["join"]
    (work / "evidence.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f'{records["calls"]} OTP/native ordered-clause calls; later-body diagnostics and retry passed.')


if __name__ == "__main__":
    main()
