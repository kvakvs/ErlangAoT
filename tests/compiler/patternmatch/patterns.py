"""Exercise semantic patterns through both CLI modes against the pinned OTP source/oracle."""
import json
import pathlib
import re
import subprocess
import sys
from bindings import compile_case, helpers
from evidence import digest, provenance, run, verify_manifest

ERRORS = r"illegal pattern|illegal expression in pattern|unbound variable|unsafe variable|invalid binary|conflicting binary|UTF binary|literal string pattern|unsized binary|map pattern requires"


def cases(source, otp, work):
    """Label suite adaptations separately from unchanged helpers and hash every generated source."""
    rows = json.loads((source / "tests/fixtures/patternmatch/patterns.json").read_text(encoding="utf-8"))
    map_path = otp / "lib/compiler/test/map_SUITE.erl"
    license_text = map_path.read_text(encoding="utf-8").split("-module(")[0]
    terms = []
    for row in rows:
        name = row["name"]
        text = f'-module({name}).\n-export([f/{row["arity"]}]).\n{row["body"]}\n'
        # Put the preserved notice after the function to keep authored diagnostic line checks stable.
        (work / f"{name}.erl").write_text(text + license_text, encoding="utf-8")
        row["sha256"] = digest(work / f"{name}.erl")
        expected = "rejected" if row["diagnostic"] else "accepted"
        terms.append(f'{{{name}, {expected}, {row["otp_diagnostic"] or "none"}}}.')
    helper_record = helpers(otp, work)
    rows.append(dict(name="bindings_otp", diagnostic="", capability="pattern matching"))
    terms.append("{bindings_otp, accepted, none}.")
    binary_path = otp / "lib/compiler/test/bs_size_expr_SUITE.erl"
    binary_source = binary_path.read_text(encoding="utf-8")
    clause = re.search(r"^do_basic_1\(.*?^    no_match\.", binary_source, re.M | re.S).group()
    wrapped = binary_source.split("-module(")[0] + "-module(patterns_binary).\n-export([do_basic_1/1]).\n" + clause + "\n"
    (work / "patterns_binary.erl").write_text(wrapped, encoding="utf-8")
    rows.append(dict(name="patterns_binary", diagnostic="", capability="pattern matching"))
    terms.append("{patterns_binary, accepted, none}.")
    (work / "patterns.term").write_text("\n".join(terms) + "\n", encoding="utf-8")
    return rows, {"match": helper_record, "binary": {"source": str(binary_path.relative_to(otp)),
        "source_sha256": digest(binary_path), "function": "do_basic_1/1", "clause": clause,
        "adaptations": "unchanged complete clauses; new module/export; retained license", "sha256": digest(work / "patterns_binary.erl")},
        "map": {"source": str(map_path.relative_to(otp)), "source_sha256": digest(map_path),
        "function": "t_key_expressions/1", "adaptations": "pat_key_tuple/call/binary/badarith move key patterns to body matches with explicit incoming parameters; retain element/2 plus its dependent binary value pattern; replace other literal value constraints by captures; specialize failing division key to 1 div 0; remove surrounding harness/closures/case dispatch; preserve license"}}


def policies(tool, work, rows):
    """Repeat semantic/capability rejection through both batch modes and all optimization policies."""
    rejected = [row for row in rows if row["diagnostic"] or row["capability"]]
    for policy in [["-O0"], ["-O0", "--no-type-specialization"], ["-O2"], ["-O2", "--no-type-specialization"]]:
        for project in [False, True]:
            for row in rejected:
                compile_case(tool, work, row, policy, project)


def accepted(tool, work, rows):
    """Ensure every OTP-accepted form is free of erroneous pattern or binding diagnostics."""
    for row in rows:
        result = subprocess.run([tool, str(work / (row["name"] + ".erl"))], capture_output=True,
                                text=True, encoding="utf-8", timeout=30)
        if not row["diagnostic"]:
            assert not re.search(ERRORS, result.stderr), (row, result.stderr)


def cli(tool, work, rows):
    """Every rejected batch preserves prior output; a valid batch subsequently recovers."""
    out = work / "out"
    out.mkdir(exist_ok=True)
    for child in out.iterdir():
        if child.is_file():
            child.unlink()
    (out / "sentinel").write_bytes(b"preserve")
    policies(tool, work, rows)
    accepted(tool, work, rows)
    assert (out / "sentinel").read_bytes() == b"preserve"
    compile_case(tool, work, next(row for row in rows if row["name"] == "pat_groups"), ["-O2"], False)


def limits(tool, work):
    """Public source depth and arithmetic ceilings reject cleanly, including after an earlier valid form."""
    for name, body, diagnostic in [
        ("depth", "f(" + "{" * 800 + "X" + "}" * 800 + ") -> X.", "nesting budget exhausted"),
        ("constant", "f({1 bsl 1000001}) -> ok.", "pattern constant limit exceeded"),
        ("digits", "f({1 bsl 40000}) -> ok.", "pattern constant limit exceeded"),
        ("wide", "f({" + ",".join(["1 bsl 30000"] * 200) + "}) -> ok.", "binding analysis work limit exceeded")]:
        path = work / (name + ".erl")
        path.write_text(f"-module({name}).\ng(X) -> X.\n{body}\n", encoding="utf-8")
        result = subprocess.run([tool, "--emit", "obj", "--artifact-dir", str(work / "limits-out"), str(path)],
                                capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert result.returncode == 1 and diagnostic in result.stderr, result.stderr
        assert not (work / "limits-out").exists()


def suites(tool, otp):
    """Original suite parsing is syntax evidence only; selected helpers get independent lint comparison."""
    options = ["-I", str(otp / "lib/compiler/src"), "--enable-feature", "maybe_expr", "--disable-feature", "compr_assign"]
    for app in ["stdlib", "kernel", "common_test", "syntax_tools"]:
        options += ["--app-dir", f"{app}={otp / 'lib' / app}"]
    for suite in ["map_SUITE", "bs_size_expr_SUITE"]:
        run([tool, "--parse-check", *options, str(otp / "lib/compiler/test" / (suite + ".erl"))])


def catalog(source, otp):
    """Keep embedded-expression legality synchronized with the exact OTP guard BIF signatures."""
    internal = (otp / "lib/stdlib/src/erl_internal.erl").read_text(encoding="utf-8")
    expected = set(re.findall(r"^(?:guard_bif|new_type_test)\(([^,]+), (\d+)\) -> true;", internal, re.M))
    implementation = (source / "compiler/src/semantic/pattern_calls.cpp").read_text(encoding="utf-8")
    actual = set(re.findall(r'\{U"([^"]+)", (\d+)\}', implementation))
    assert actual == expected, (expected - actual, actual - expected)


def main():
    tool, root, otp_root, directory, escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    provenance(source, otp, source / "tests/fixtures/patternmatch", work)
    fixtures = source / "tests/fixtures/patternmatch"
    verify_manifest(otp, (fixtures / "pattern-otp.tsv").read_text(encoding="utf-8"))
    verify_manifest(fixtures, (fixtures / "pattern-fixtures.tsv").read_text(encoding="utf-8"))
    catalog(source, otp)
    rows, records = cases(source, otp, work)
    oracle = run([escript, str(source / "tests/compiler/patternmatch/patterns.escript"), str(work)])
    cli(tool, work, rows)
    limits(tool, work)
    suites(tool, otp)
    (work / "evidence.json").write_text(json.dumps({"reference": json.loads((work / "provenance.json").read_text(encoding="utf-8")),
        "pattern_manifests": {name: digest(fixtures / name) for name in ["pattern-otp.tsv", "pattern-fixtures.tsv"]},
        "cases": rows, "helpers": records, "oracle": oracle,
        "policies": "O0/O2; specialization on/off; positional/project; nonpublication/recovery",
        "execution": "deferred; existing native identity/projection workflows remain required"}, indent=2) + "\n", encoding="utf-8")
    print(oracle + "Pattern legality, source limits, original-suite parsing and batch nonpublication passed.")


if __name__ == "__main__":
    main()
