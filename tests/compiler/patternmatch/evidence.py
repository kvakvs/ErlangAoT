"""Verify pinned semantic evidence, OTP acceptance, and the immediate native baseline."""
import csv
import hashlib
import json
import pathlib
import re
import subprocess
import sys


def run(command):
    """Retain both streams and the failing command in test diagnostics."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=240)
    if result.returncode:
        raise AssertionError(f"{command}: {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def digest(path):
    """Hash canonical LF text so Windows checkout conversion cannot change source identity."""
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def verify_manifest(root, manifest):
    """Refuse any changed evidence before parsing or execution."""
    for row in csv.DictReader(manifest.splitlines(), delimiter="\t"):
        actual = digest(root / row["path"])
        if actual != row["sha256"]:
            raise AssertionError(f"Stale evidence hash: {row['path']}: {actual}")


def provenance(source, otp, fixtures, work):
    """Check the pin, exact evidence bytes, authored fixtures and a deliberately stale hash."""
    revision = run(["git", "-C", str(otp), "rev-parse", "HEAD"]).strip()
    pin = (source / "references/otp-pin.cmake").read_text(encoding="utf-8")
    assert revision in pin, f"OTP checkout is not pinned: {revision}"
    run(["git", "-C", str(otp), "diff", "--exit-code", "HEAD", "--",
         "lib/stdlib/src", "lib/compiler", "lib/stdlib/include", "lib/kernel/include",
         "lib/common_test/include", "lib/syntax_tools/include", "system/doc/reference_manual"])
    manifest = (fixtures / "otp.tsv").read_text(encoding="utf-8")
    verify_manifest(otp, manifest)
    verify_manifest(fixtures, (fixtures / "fixtures.tsv").read_text(encoding="utf-8"))
    stale = manifest.replace(manifest.splitlines()[1].split("\t")[0], "0" * 64, 1)
    try:
        verify_manifest(otp, stale)
    except AssertionError as error:
        assert "Stale evidence hash:" in str(error)
    else:
        raise AssertionError("Deliberately stale hash was accepted")
    (work / "provenance.json").write_text(json.dumps({
        "revision": revision, "branch": "maint-29", "hash_policy": "SHA256 UTF-8 source bytes with CRLF normalized to LF",
        "stale_hash": "rejected", "otp_manifest_sha256": digest(fixtures / "otp.tsv"),
        "fixtures_manifest_sha256": digest(fixtures / "fixtures.tsv")}, indent=2) + "\n", encoding="utf-8")


def catalog(otp, fixtures):
    """Require a reviewed owner/rejection row for every pinned guard signature and operator."""
    internal = (otp / "lib/stdlib/src/erl_internal.erl").read_text(encoding="utf-8")
    groups = "guard_bif|new_type_test|old_type_test|arith_op|bool_op|comp_op"
    expected = set(re.findall(rf"^({groups})\(([^,]+), (\d+)\) -> true;", internal, re.M))
    rows = list(csv.DictReader((fixtures / "guards.tsv").read_text(encoding="utf-8").splitlines(), delimiter="\t"))
    actual = {(row["category"], row["name"], row["arity"]) for row in rows}
    assert actual == expected, f"Guard catalog drift: missing={expected - actual}, extra={actual - expected}"
    assert len(actual) == len(rows), "Duplicate guard rows"
    assert all(row["owner"] and row["rejection"] for row in rows)


def suites(tool, otp, work):
    """Parse original suites with their real headers; never call this executable coverage."""
    options = ["-I", str(otp / "lib/compiler/src"), "--enable-feature", "maybe_expr",
               "--disable-feature", "compr_assign"]
    for app in ["stdlib", "kernel", "common_test", "syntax_tools"]:
        options += ["--app-dir", f"{app}={otp / 'lib' / app}"]
    results = []
    for name in ["guard_SUITE", "match_SUITE", "trycatch_SUITE"]:
        path = otp / "lib/compiler/test" / (name + ".erl")
        run([tool, "--preprocess-check", *options, str(path)])
        tree = run([tool, "--print-ast", *options, str(path)])
        assert tree.strip(), f"Empty AST: {name}"
        results.append({"source": str(path.relative_to(otp)), "source_sha256": digest(path),
                        "preprocessing": "accepted", "syntax": "accepted", "execution": "not attempted"})
    (work / "suites.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


def native(tool, cmake, source, work, settings, config, suffix):
    """Run the existing separately linked consumer with four optimization/specialization policies."""
    (work / "native-project.toml").write_bytes(b'schema_version=1\n[[targets]]\nname="native"\nsources=["answer.erl","client.erl"]\n')
    for level, extra, name in [("O0", "", "O0"), ("O0", "--no-type-specialization", "O0-off"),
                               ("O2", "", "O2"), ("O2", "--no-type-specialization", "O2-off")]:
        for project in [False, True]:
            run([cmake, f"-DTOOL={tool}", f"-DPROJECT_MODE={project}", f"-DOPTIMIZATION={level}", f"-DEXTRA_OPTIONS={extra}",
                 f"-DSOURCE_ROOT={source.as_posix()}", f"-DTEST_DIR={(work / name).as_posix()}",
                 f"-DINPUT_ROOT={work.as_posix()}", f"-DHOST_SETTINGS={settings}",
                 f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}",
                 "-P", str(source / "tests/compiler/codegen/native.cmake")])


def audit_main():
    """Publish auditable evidence under the build directory without updating tracked expectations."""
    tool, cmake, root, otp_root, directory, settings, config, suffix, escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    fixtures = source / "tests/fixtures/patternmatch"
    work.mkdir(parents=True, exist_ok=True)
    provenance(source, otp, fixtures, work)
    catalog(otp, fixtures)
    suites(tool, otp, work)
    from authored import stage
    stage(source, 'baseline', work)
    oracle = run([escript, str(source / "tests/compiler/patternmatch/oracle.escript"), str(fixtures), str(work)])
    (work / "oracle.txt").write_text(oracle, encoding="utf-8")
    native(tool, cmake, source, work, settings, config, suffix)
    print(oracle + "Three original suites parsed; stale hash rejected; local helpers executed in four native modes.")


def main():
    """Execute project-owned baseline fixtures without an OTP installation or checkout."""
    from stored import load
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    load(source, 'baseline', work)
    native(tool, cmake, source, work, settings, config, suffix)
    print('Stored OTP projection baseline passed in four native policies.')


if __name__ == "__main__":
    if sys.argv[1:2] == ['--audit']:
        del sys.argv[1]
        audit_main()
    else:
        main()
