"""Execute explicitly adapted OTP atom-return leaves through the public CLI and native runtime."""
import hashlib
import json
import pathlib
import re
import subprocess
import sys
from stored import load
from evidence import digest, run, verify_manifest


def fixtures(source, otp, work):
    """Retain licensing and exact provenance; only literal leaves are adapted, not guard behavior."""
    suite = otp / "lib/compiler/test/guard_SUITE.erl"
    text = suite.read_text(encoding="utf-8")
    verify_manifest(otp, (source / "tests/fixtures/patternmatch/otp.tsv").read_text(encoding="utf-8"))
    clauses = ["rb(Size, ToRead, SoFar) when SoFar + Size < 81920; ToRead == [] -> true;",
               "rb(_, _, _) -> false.", "csemi2(A, B) when tuple_size(A) > 1; tuple_size(B) > 2 -> ok;"]
    assert all(clause in text for clause in clauses), "OTP leaf provenance changed"
    license_text = text[:text.index("-module(")]
    names = ["truth", "falsity", "ok_value", "unicode", "empty", "nul", "projected"]
    answer = license_text + "-module(answer).\n-export([id/1," + ",".join(n + "/0" for n in names) + "]).\n"
    answer += "-spec truth() -> integer().\ntruth() -> true.\nfalsity() -> false.\nok_value() -> ok.\n"
    answer += "unicode() -> '\u03bb\U0001f600'.\nempty() -> ''.\nnul() -> 'a\\x{0}b'.\nid(X) -> X.\nprojected() -> id((true)).\n"
    client = "-module(client).\n-export([same/0," + ",".join(n + "/0" for n in names) + "]).\nsame() -> true.\n"
    client += "".join(f"{n}() -> answer:id(answer:{n}()).\n" for n in names)
    for name, contents in [("answer", answer), ("client", client)]:
        (work / (name + ".erl")).write_bytes(contents.encode("utf-8"))
    (work / "project.toml").write_text("schema_version=1\n[[targets]]\nname='atoms'\nsources=['answer.erl','client.erl']\n",
                                       encoding="utf-8")
    record = {"revision": run(["git", "-C", str(otp), "rev-parse", "HEAD"]).strip(),
              "source": suite.relative_to(otp).as_posix(), "source_sha256": digest(suite), "functions": ["rb/3", "csemi2/2"],
              "selected_clauses": clauses, "clause_sha256": hashlib.sha256("\n".join(clauses).encode()).hexdigest(),
              "adaptation": "Extract only true/false/ok return leaves into truth/0, falsity/0, ok_value/0. "
                            "No rb/csemi2 guard or dispatch behavior is claimed. Add authored Unicode, empty, NUL, "
                            "identity, grouping and cross-module calls, plus a deliberately wrong integer spec.",
              "declarations": {name: re.findall(r"^-(?:module|export|spec).*", text, re.M)
                               for name, text in [("answer", answer), ("client", client)]},
              "generated_sha256": {name: digest(work / (name + ".erl")) for name in ["answer", "client"]}}
    (work / "helpers.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


def failed_batch(tool, work):
    """An unsupported later source must leave no earlier atom artifact, followed by a successful retry."""
    bad = work / "bad.erl"
    bad.write_text("-module(bad). value() -> [X || X <- []].\n", encoding="utf-8")
    output = work / "failed"
    output.mkdir(exist_ok=True)
    for file in output.iterdir():
        if file.is_file():
            file.unlink()
    sentinel = output / "sentinel"
    sentinel.write_text("preserved", encoding="utf-8")
    command = [tool, "--emit", "obj", "--artifact-dir", str(output), str(work / "answer.erl"), str(bad)]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=30)
    assert result.returncode == 1 and "[heap expressions] notimpl" in result.stderr, result
    assert list(output.iterdir()) == [sentinel] and sentinel.read_text() == "preserved"
    run(command[:-1])
    assert len(list(output.iterdir())) == 2


def main():
    """Compare all four optimization/specialization modes and both source entry paths with stored OTP results."""
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    load(source, "atoms", work)
    # Make repeated CTest runs deterministic without deleting unrelated build artifacts.
    failed = work / "failed"
    for path in failed.glob("eav1_*"):
        path.unlink()
    failed_batch(tool, work)
    for level in ["O0", "O2"]:
        for disabled in [False, True]:
            name = level + ("-off" if disabled else "-on")
            run([cmake, f"-DTOOL={tool}", f"-DOPTIMIZATION={level}",
                 "-DEXTRA_OPTIONS=" + ("--no-type-specialization" if disabled else ""),
                 f"-DPROJECT_MODE={'ON' if disabled else 'OFF'}", f"-DSOURCE_ROOT={source.as_posix()}",
                 f"-DTEST_DIR={(work / name).as_posix()}", f"-DINPUT_ROOT={work.as_posix()}",
                 f"-DHOST_SETTINGS={settings}", f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}",
                 "-P", str(source / "tests/compiler/codegen/atoms.cmake")])
    print("OTP atom spellings/booleans match all four native policies; ownership, limits, rollback and retry pass.")


if __name__ == "__main__":
    main()
