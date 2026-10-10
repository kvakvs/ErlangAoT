"""Audit catalog-owned CLI capability paths for deferred compiler features."""
import pathlib
import re
import subprocess
import sys

CASES = {
    "heap expressions": "-feature(compr_assign, enable). f(L) -> [Y || X <- L, Y = X].",
    "dynamic calls": "f() -> fun erlang:apply/2.",
    "behavior-changing attributes": "-on_load(f/0). f() -> 1.",
}
# Deferred features that only warn: compilation continues without them.
WARNINGS = {
    "parse transforms": "-compile({parse_transform, x}). f() -> 1.",
}


def check(tool, work, feature, options, project, verbose):
    """Require one exact catalog marker with context; a failure preserves existing destinations."""
    args = [tool, *options] + (["--verbose"] if verbose else [])
    args += ["--project", str(work / "project.toml")] if project else [str(work / "sample.erl")]
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=30)
    marker = f"[{feature}] notimpl"
    warning = feature in WARNINGS
    assert result.returncode == (0 if warning else 1) and not result.stdout, (args, result)
    assert result.stderr.count(marker) == 1, (args, result.stderr)
    if warning:
        assert "warning: " in result.stderr and "error: " not in result.stderr, result.stderr
    assert re.search(r"sample\.erl:\d+:\d+:", result.stderr), result.stderr
    assert '[module="sample"]' in result.stderr, result.stderr
    if project:
        assert "audit" in result.stderr, result.stderr
    assert (work / "output/sentinel").read_text(encoding="utf-8") == "preserved"
    if not warning:
        assert sorted(p.name for p in (work / "output").iterdir()) == ["sentinel"]


def main():
    """Make missing catalog coverage fail when a compiler-owned feature is added."""
    tool, directory, catalog_path = sys.argv[1:]
    work = pathlib.Path(directory)
    (work / "output").mkdir(parents=True, exist_ok=True)
    for file in (work / "output").iterdir():
        if file.is_file():
            file.unlink()
    (work / "output/sentinel").write_text("preserved", encoding="utf-8")
    catalog = pathlib.Path(catalog_path).read_text(encoding="utf-8")
    entries = re.findall(r'FeatureInfo\{.*?\.name = "([^"]+)".*?\.owner = FeatureOwner::(\w+).*?\.status = FeatureStatus::(\w+).*?\}', catalog, re.S)
    deferred = {name for name, owner, status in entries if owner == "compiler" and status == "deferred"}
    assert deferred == set(CASES) | set(WARNINGS), deferred
    (work / "project.toml").write_text("schema_version=1\n[[targets]]\nname='audit'\nsources=['sample.erl']\n",
                                        encoding="utf-8")
    for feature, body in (CASES | WARNINGS).items():
        (work / "sample.erl").write_text("-module(sample).\n" + body + "\n", encoding="utf-8")
        for level in ["-O0", "-O2"]:
            options = [level, "--emit", "obj", "--artifact-dir", str(work / "output")]
            for project in [False, True]:
                for verbose in [False, True]:
                    check(tool, work, feature, options, project, verbose)


if __name__ == "__main__":
    main()
