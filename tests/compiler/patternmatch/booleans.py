"""Compare guard grouping and term-valued boolean execution with OTP through real CLI objects."""
import itertools
import json
import pathlib
import re
import sys
from stored import load
from evidence import digest, provenance, run
from immediate import native
from services import write_calls

VALUES = ["true", "false", "glurf", 0, 7, "nil", "tuple", "Ω"]
OPERATORS = [("lazy_and", "andalso"), ("lazy_or", "orelse"), ("strict_and", "and"),
             ("strict_or", "or"), ("strict_xor", "xor")]


def kernels(source, otp, work):
    """Retain suite guard constraints and boolean expressions, labeling changed contexts and removed fallbacks."""
    fixture = source / "tests/fixtures/patternmatch/booleans.json"
    data = json.loads(fixture.read_text(encoding="utf-8"))
    guard = otp / "lib/compiler/test/guard_SUITE.erl"
    andor = otp / "lib/compiler/test/andor_SUITE.erl"
    text = guard.read_text(encoding="utf-8")
    boolean_text = andor.read_text(encoding="utf-8")
    for witness in ["if True; Glurf -> ok", "if element(42, ATuple); True -> ok"]:
        assert witness in text, witness
    for witness in ["false = false andalso exit(exit_now)", "true = true orelse exit(exit_now)",
                    "false = id(false) andalso not id(glurf)", "true = id(true) orelse not id(glurf)"]:
        assert witness in boolean_text, witness
    definitions = data["functions"]
    suites = []
    for name in ["csemi4_orelse_a", "csemi4_orelse_b", "csemi4_orelse_c", "csemi4_orelse_d"]:
        clause = re.search(r"^" + name + r"\(A, X, B, Y\) when .*?-> ok;", text, re.M | re.S).group()
        definitions.append([name, 4, clause[:-1] + "."])
        suites.append({"function": name + "/4", "clause_sha256": digest_text(clause),
                       "adaptation": "complete first guard/body retained; fallback removed until step 9"})
    for name, operator in OPERATORS:
        definitions.extend([[name, 2, f"{name}(X,Y) -> X {operator} Y."],
                            ["guard_" + name, 2, f"guard_{name}(X,Y) when X {operator} Y -> ok."]])
    definitions += [["not_value", 1, "not_value(X) -> not X."],
                    ["guard_not", 1, "guard_not(X) when not X -> ok."],
                    ["wide_semi", 1, "wide_semi(X) when " + "; ".join(["X"] * 128 + ["true"]) + " -> ok."],
                    ["wide_comma", 1, "wide_comma(X) when " + ", ".join(["true"] * 128 + ["X"]) + "; true -> ok."],
                    ["deep_lazy", 1, "deep_lazy(X) -> " + "(" * 96 + "X" + " andalso true)" * 96 + "."]]
    exports = ",".join(f"{name}/{arity}" for name, arity, _ in definitions)
    (work / "answer.erl").write_bytes((text.split("-module(")[0] + f"-module(answer).\n-export([{exports}]).\n" +
        "\n".join(body for _, _, body in definitions) + "\n").encode())
    (work / "client.erl").write_bytes(b"-module(client).\n-compile({no_auto_import,[is_integer/1]}).\n"
        b"-export([nested_skip/1,nested_reach/1,strict/1,local_shadow/1,qualified/1]).\n"
        b"nested_skip(X) -> true orelse answer:required_zero(X).\n"
        b"nested_reach(X) -> false orelse answer:required_zero(X).\n"
        b"strict(X) -> true or answer:required_zero(X).\n"
        b"local_shadow(X) -> is_integer(X).\nqualified(X) -> erlang:is_integer(X).\nis_integer(X) -> X.\n")
    calls = []
    for name, _ in OPERATORS:
        for left, right in itertools.product(VALUES, repeat=2):
            calls += [("answer", name, [left, right]), ("answer", "guard_" + name, [left, right])]
    for name in ["semicolon", "comma", "alias", "term_join", "nested", "nested_semi", "not_and", "not_or",
                 "qualified_and", "wrong_spec"]:
        calls += [("answer", name, list(args)) for args in itertools.product(VALUES, repeat=2)]
    calls += [("answer", "mixed", list(args)) for args in itertools.product(VALUES, repeat=3)]
    for name in ["head_first", "not_value", "guard_not", "qualified_not", "required_zero", "local_skip", "local_reach",
                 "wide_semi", "wide_comma", "deep_lazy"]:
        calls += [("answer", name, [value]) for value in VALUES]
    calls += [("answer", name, [0]) for name in data["single_calls"]]
    for item in suites:
        name = item["function"].split("/")[0]
        calls += [("answer", name, list(args)) for args in itertools.product(["nil", "tuple", "glurf"], [0, 2],
                                                                                 ["nil", "tuple", "glurf"], [0, 2])]
    for name in ["nested_skip", "nested_reach", "strict", "local_shadow", "qualified"]:
        calls += [("client", name, [value]) for value in VALUES]
    write_calls(work, calls)
    return {"calls": len(calls), "sources": {str(p.relative_to(otp)): digest(p) for p in [guard, andor]},
        "functions": suites, "other_functions": "guard_SUITE:semicolon/1,comma/1; andor_SUITE:t_andalso/1,t_orelse/1",
        "adaptations": "if/comprehension/Common Test wrappers replaced by complete single-clause functions; retain variable, nonboolean, strict/lazy and reached-error behavior; exhaustion observed as function_clause; failing exit operand uses admitted hd([]) or real generated-clause failure; float/heap/assignment-dependent branches excluded explicitly",
        "declarations": exports, "fixture_sha256": digest(fixture),
        "wrapper_sha256": digest(work / "answer.erl"), "client_sha256": digest(work / "client.erl")}


def digest_text(text):
    """Record the exact retained clause before changing only its fallback separator."""
    import hashlib
    return hashlib.sha256(text.encode()).hexdigest()


def inspection(tool, work):
    """Check word joins and lazy RHS blocks in verified IR; label both-width objects as inspection only."""
    source = work / "answer.erl"
    for mode in ["--print-ir", "--print-optimized-ir", "--print-types"]:
        options = [] if mode == "--print-types" else ["-O2"]
        result = run([tool, *options, mode, str(source), str(work / "client.erl")])
        if mode == "--print-ir":
            assert "lazy.right" in result and "lazy.join" in result and "phi i64" in result, result
            assert "guard.alternative" in result and "guard.next" in result, result
    widths = []
    (work / "width.erl").write_bytes(b"-module(width). -export([f/2,g/1]).\nf(X,Y) -> X andalso Y. g(X) -> not X.\n")
    for bits, triple in [(32, "i686-pc-windows-msvc"), (64, "x86_64-pc-windows-msvc")]:
        ir = run([tool, "--target-triple", triple, "--print-ir", str(work / "width.erl")])
        assert f"phi i{bits}" in ir and "erlang_aot_immediate_v1" in ir, ir
        run([tool, "--target-triple", triple, "--emit", "obj", "--artifact-dir", str(work / f"width{bits}"),
             str(work / "width.erl")])
        widths.append({"bits": bits, "triple": triple, "objects_and_verified_ir": "passed", "execution": "not attempted"})
    return widths


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "booleans", work)
    native(tool, cmake, source, work, settings, config, suffix)
    records["widths"] = inspection(tool, work)
    (work / "evidence.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f'{records["calls"]} OTP/native guard/boolean calls; four policies, grouping, word joins, errors and recovery passed.')


if __name__ == "__main__":
    main()
