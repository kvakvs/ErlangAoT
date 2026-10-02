"""Compare scoped binding legality through the CLI and retain pinned OTP helpers."""
import json
import pathlib
import re
import subprocess
import sys
from evidence import digest, native, provenance, run
from stored import load

BINDING_ERRORS = r"unbound variable|unsafe variable|wildcard '_' cannot be read|guards cannot bind variables"


def source_cases(source, work):
    """Keep authored scope cases and oracle expectations in an auditable generated manifest."""
    path = source / "tests/fixtures/patternmatch/bindings.json"
    rows = json.loads(path.read_text(encoding="utf-8"))
    terms = []
    for row in rows:
        name = row["name"]
        text = f'-module({name}).\n-export([f/{row["arity"]}]).\n{row["body"]}\n'
        (work / f"{name}.erl").write_bytes(text.encode())
        accepted = "rejected" if row["diagnostic"] else "accepted"
        terms.append(f'{{{name}, {accepted}, {row["otp_diagnostic"] or "none"}}}.')
        row["sha256"] = digest(work / f"{name}.erl")
    return rows, terms


def helpers(otp, work):
    """Extract complete match_SUITE clauses unchanged, including their original license."""
    path = otp / "lib/compiler/test/match_SUITE.erl"
    text = path.read_text(encoding="utf-8")
    names = [("gh_6516_scope1", 0), ("gh_6516_scope2", 0), ("mutable_variables_1", 0),
             ("match_right_tuple_1", 1), ("force_succ_regs", 2), ("id", 1)]
    records, clauses = [], []
    for name, arity in names:
        found = re.findall(rf"^{name}[(][^\n]*?[)] ->.*?\.\s*$", text, re.M | re.S)
        assert len(found) == 1, (name, found)
        clause = found[0].strip()
        clauses.append(clause)
        records.append({"function": f"{name}/{arity}", "clause": clause})
    license_text = text[:text.index("-module(")]
    exports = ",".join(f"{name}/{arity}" for name, arity in names)
    wrapped = license_text + f"-module(bindings_otp).\n-export([{exports}]).\n" + "\n".join(clauses) + "\n"
    (work / "bindings_otp.erl").write_bytes(wrapped.encode())
    for module, clause, export in [("answer", clauses[-2], "force_succ_regs/2"), ("client", clauses[-1], "id/1")]:
        extra = ""
        if module == "answer":
            export += ",identity/1"
            extra = "-spec force_succ_regs(integer(), atom()) -> atom().\n"
            clause += "\nidentity(X) -> client:id(X)."
        wrapper = license_text + f"-module({module}).\n-export([{export}]).\n" + extra + clause + "\n"
        (work / f"{module}.erl").write_bytes(wrapper.encode())
    (work / "calls.txt").write_bytes(b"answer force_succ_regs 2 17 -42\nclient id 1 -134217728\nanswer identity 1 -7\n")
    (work / "expected.txt").write_bytes(b"-42\n-134217728\n-7\n")
    return {"source": str(path.relative_to(otp)), "source_sha256": digest(path),
            "helpers": records, "adaptations": "unchanged complete clauses; new modules/exports; retained license; native answer adds identity(X) -> client:id(X) and misleading projection spec",
            "wrapper_sha256": digest(work / "bindings_otp.erl")}


def compile_case(tool, work, row, policy, project):
    """Reject semantic errors or deferred capabilities before either batch mode publishes anything."""
    name = row["name"]
    args = [tool, *policy, "--emit", "obj"]
    if project:
        manifest = f'schema_version=1\n[[targets]]\nname="bindings"\nsources=["client.erl","{name}.erl"]\noutput="out"\n'
        (work / "project.toml").write_text(manifest, encoding="utf-8")
        args += ["--project", "project.toml"]
    else:
        args += ["--artifact-dir", "out", "client.erl", name + ".erl"]
    result = subprocess.run(args, cwd=work, capture_output=True, text=True, encoding="utf-8", timeout=30)
    rejected = bool(row["diagnostic"] or row["capability"])
    assert result.returncode == int(rejected), (args, result.stdout, result.stderr)
    assert not result.stdout, result.stdout
    if row["diagnostic"]:
        assert row["diagnostic"] in result.stderr, result.stderr
        assert re.search(rf'{name}\.erl:3:\d+:.*{re.escape(row["diagnostic"])}', result.stderr), result.stderr
    else:
        assert not re.search(BINDING_ERRORS, result.stderr), result.stderr
    if row["capability"] and not row["diagnostic"]:
        assert row["capability"] in result.stderr, result.stderr
    if rejected:
        assert sorted(p.name for p in (work / "out").iterdir()) == ["sentinel"], list((work / "out").iterdir())


def cli(tool, work, rows):
    """Run negative batches at both optimization levels and specialization settings, then prove recovery."""
    out = work / "out"
    out.mkdir(exist_ok=True)
    for child in out.iterdir():
        if child.is_file():
            child.unlink()
    (out / "sentinel").write_bytes(b"preserve")
    rejected = [row for row in rows if row["diagnostic"] or row["capability"]]
    for policy in [["-O0"], ["-O0", "--no-type-specialization"], ["-O2"], ["-O2", "--no-type-specialization"]]:
        for project in [False, True]:
            for row in rejected:
                compile_case(tool, work, row, policy, project)
    assert (out / "sentinel").read_bytes() == b"preserve"
    compile_case(tool, work, rows[0], ["-O2"], False)
    report = run([tool, "--print-types", str(work / "identity.erl")])
    assert "binding=clause[0].local[0]" in report and "argument[0]" in report
    assert "term() [unknown]" in report


def locations(tool, work):
    """Use a real include and macro expansion to preserve physical diagnostic provenance."""
    (work / "origin.hrl").write_bytes(b"f() -> Missing.\n")
    (work / "origin.erl").write_bytes(b'-module(origin).\n-include("origin.hrl").\n')
    result = subprocess.run([tool, str(work / "origin.erl")], capture_output=True, text=True, encoding="utf-8")
    assert result.returncode == 1 and "origin.hrl:1:8:" in result.stderr, result.stderr
    (work / "macro.erl").write_bytes(b'-module(macro).\n-define(READ, Missing).\nf() -> ?READ.\n')
    result = subprocess.run([tool, str(work / "macro.erl")], capture_output=True, text=True, encoding="utf-8")
    assert result.returncode == 1 and "unbound variable Missing" in result.stderr, result.stderr
    assert "macro.erl:3:" in result.stderr and "macro" in result.stderr, result.stderr


def main():
    """Separate legality, private binding invariants and executable projection evidence."""
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, 'bindings', work)
    rows, helper_record, oracle = records['cases'], records['helpers'], records['oracle']
    cli(tool, work, rows)
    locations(tool, work)
    native(tool, cmake, source, work, settings, config, suffix)
    (work / "evidence.json").write_text(json.dumps({"cases": rows, "otp_helpers": helper_record,
        "oracle": oracle, "native": "unchanged force_succ_regs/2 and id/1; four policies",
        "deferred": "step 9 clause isolation and failed-candidate rollback execution"}, indent=2) + "\n", encoding="utf-8")
    print(oracle + "Binding legality and nonpublication passed in both CLI modes; native projections passed four policies.")


if __name__ == "__main__":
    main()
