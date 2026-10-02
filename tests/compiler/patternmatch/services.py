"""Exercise semantic guard resolution and immediate services through the public CLI and OTP."""
import json
import pathlib
import re
import subprocess
import sys
from stored import load
from evidence import digest, provenance, run
from immediate import native, token, erl
from bindings import compile_case

PREDICATES = ["is_atom", "is_integer", "is_number", "is_boolean", "is_tuple", "is_list", "is_binary",
              "is_bitstring", "is_float", "is_map", "is_pid", "is_port", "is_reference", "is_function"]
OPERATORS = [("exact", "=:="), ("exact_ne", "=/="), ("equal", "=="), ("unequal", "/="),
             ("less", "<"), ("leq", "=<"), ("greater", ">"), ("geq", ">=")]
VALUES = [-576460752303423488, -1, 0, 576460752303423487, "z", "a", "true", "false", "Ω", "nil", "tuple"]


def kernels(otp, work):
    """Adapt single immediate guard tests and number classifications; preserve exact constraints and notices."""
    suite = otp / "lib/compiler/test/guard_SUITE.erl"
    text = suite.read_text(encoding="utf-8")
    assert "bool(X) when is_boolean(X) -> ok;" in text
    numbers = (otp / "lib/compiler/test/beam_type_SUITE.erl").read_text(encoding="utf-8")
    assert "Int = id(42),\n    true = is_integer(Int),\n    true = is_number(Int),\n    false = is_float(Int)" in numbers
    overridden = (otp / "lib/compiler/test/overridden_bif_SUITE.erl").read_text(encoding="utf-8")
    assert "-compile({no_auto_import,[is_reference/1,size/1]})." in overridden
    assert "-import(gb_sets, [size/1])." in overridden
    definitions = [(f"body_{name}", 1, f"body_{name}(X) -> {name}(X).") for name in PREDICATES]
    definitions += [(f"guard_{name}", 1, f"guard_{name}(X) when {name}(X) -> ok.") for name in PREDICATES]
    definitions += [(name, 2, f"{name}(X,Y) -> X {operator} Y.") for name, operator in OPERATORS]
    definitions += [("bool", 1, "bool(X) when is_boolean(X) -> ok."),
        ("legacy_integer", 1, "legacy_integer(X) when integer(X) -> X."),
        ("legacy_float", 1, "legacy_float(X) when float(X) -> X."),
        ("function_arity", 2, "function_arity(X,N) -> is_function(X,N)."),
        ("guard_arity", 2, "guard_arity(X,N) when is_function(X,N) -> ok."),
        ("tuple_size_value", 1, "tuple_size_value(X) -> tuple_size(X)."),
        ("length_value", 1, "length_value(X) -> length(X)."), ("size_value", 1, "size_value(X) -> size(X)."),
        ("head_value", 1, "head_value(X) -> hd(X)."), ("tail_value", 1, "tail_value(X) -> tl(X)."),
        ("element_value", 2, "element_value(X,Y) -> element(X,Y)."),
        ("guard_element", 2, "guard_element(X,Y) when element(X,Y) -> ok."),
        ("min_value", 2, "min_value(X,Y) -> min(X,Y)."), ("max_value", 2, "max_value(X,Y) -> max(X,Y)."),
        ("wrong_spec", 1, "-spec wrong_spec(integer()) -> boolean().\nwrong_spec(X) -> is_integer(X)."),
        ("head_first", 1, "head_first(ok) when element(1,{}) -> impossible."),
        ("qualified", 1, "qualified(X) when erlang:is_atom(X) -> X.")]
    exports = ",".join(f"{name}/{arity}" for name, arity, _ in definitions)
    (work / "answer.erl").write_bytes((text.split("-module(")[0] + f"-module(answer).\n-export([{exports}]).\n" +
        "\n".join(body for _, _, body in definitions) + "\n").encode())
    (work / "client.erl").write_bytes(b"-module(client).\n-export([nested/1,retry/1]).\nnested(X) -> answer:is_atom(answer:head_value(X)).\nretry(X) -> answer:qualified(X).\n".replace(b"answer:is_atom", b"answer:body_is_atom"))
    calls = []
    for predicate in PREDICATES:
        for value in VALUES:
            calls.extend([("answer", "body_" + predicate, [value]), ("answer", "guard_" + predicate, [value])])
    for name, _ in OPERATORS:
        calls += [("answer", name, [left, right]) for left in VALUES for right in VALUES]
    for name in ["bool", "legacy_integer", "legacy_float", "tuple_size_value", "length_value", "size_value",
                 "head_value", "tail_value", "wrong_spec", "head_first", "qualified"]:
        calls += [("answer", name, [value]) for value in VALUES]
    for name in ["function_arity", "guard_arity", "element_value", "guard_element"]:
        calls += [("answer", name, [value, arity]) for value in [0, "ok", "tuple"] for arity in [-1, 0, 1, "ok"]]
    for name in ["min_value", "max_value"]:
        calls += [("answer", name, [left, right]) for left in VALUES for right in VALUES]
    calls += [("client", "nested", ["nil"]), ("client", "retry", ["ok"])]
    write_calls(work, calls)
    return {"calls": len(calls), "source": str(suite.relative_to(otp)), "sha256": digest(suite),
        "function": "bool/1", "adaptations": "retain first guarded bool/1 clause; fallback deferred to step 9; modern predicate and comparison kernels adapt guard_SUITE and beam_type_SUITE:numbers/1 immediate checks without Common Test/list construction; complete authored helpers retain all tested operations",
        "declarations": exports, "wrapper_sha256": digest(work / "answer.erl"),
        "other_sources": {name: digest(otp / f"lib/compiler/test/{name}.erl") for name in ["beam_type_SUITE", "overridden_bif_SUITE"]}}


def write_calls(work, calls):
    """Write stable native arguments and independent mathematical OTP terms."""
    (work / "calls.txt").write_bytes(("\n".join(f"{module} {name} {len(args)} " + " ".join(map(token, args))
        for module, name, args in calls) + "\n").encode())
    (work / "calls.term").write_bytes(("\n".join(f"{{{module},{name},[" + ",".join(map(erl, args)) + "]}."
        for module, name, args in calls) + "\n").encode())
    (work / "project.toml").write_bytes(b'schema_version=1\n[[targets]]\nname="services"\nsources=["answer.erl","client.erl"]\n')


def resolution(tool, source, work, recorded):
    """Compare located legal/illegal/deferred calls with OTP, including skipped illegal operands."""
    cases = work / "resolution"
    (cases / "out").mkdir(parents=True, exist_ok=True)
    for file in (cases / "out").iterdir():
        file.unlink()
    (cases / "out/sentinel").write_bytes(b"preserve")
    (cases / "client.erl").write_bytes(b"-module(client). id(X) -> X.\n")
    rows = json.loads((source / "tests/fixtures/patternmatch/guard-resolution.json").read_text(encoding="utf-8"))
    terms = []
    for row in rows:
        text = f'-module({row["name"]}).\n{row.get("metadata", "")}\n{row["body"]}\n'
        (cases / (row["name"] + ".erl")).write_bytes(text.encode())
        terms.append(f'{{{row["name"]},{"rejected" if row["diagnostic"] else "accepted"},none}}.')
        if row["diagnostic"] or row["capability"]:
            for options in [["-O0"], ["-O2"], ["-O0", "--no-type-specialization"], ["-O2", "--no-type-specialization"]]:
                for project in [False, True]:
                    compile_case(tool, cases, row, options, project)
        else:
            run([tool, str(cases / (row["name"] + ".erl"))])
        row["sha256"] = digest(cases / (row["name"] + ".erl"))
    (cases / "patterns.term").write_bytes(("\n".join(terms) + "\n").encode())
    assert digest(source / "tests/fixtures/patternmatch/guard-resolution.json") == recorded['fixture_sha256']
    oracle = recorded['oracle']
    assert (cases / "out/sentinel").read_bytes() == b"preserve"
    return {"cases": rows, "oracle": oracle}


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "services", work)
    native(tool, cmake, source, work, settings, config, suffix)
    records["resolution"] = resolution(tool, source, work, records['resolution_oracle'])
    for mode in ["--print-ir", "--print-optimized-ir", "--print-types"]:
        options = [] if mode == "--print-types" else ["-O2"]
        run([tool, *options, mode, str(work / "answer.erl"), str(work / "client.erl")])
    (work / "evidence.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f'{records["calls"]} native/OTP service calls; {len(records["resolution"]["cases"])} resolution cases passed.')


if __name__ == "__main__":
    main()
