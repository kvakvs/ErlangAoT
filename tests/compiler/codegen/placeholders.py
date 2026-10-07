"""Audit catalog-owned CLI capability paths for deferred compiler features."""
import pathlib
import re
import subprocess
import sys

CASES = {
    "guards": "f(X) when self() =:= X -> X.",
    "arithmetic": "f(X) -> X ++ [].",
    "heap expressions": "-feature(compr_assign, enable). f(L) -> [Y || X <- L, Y = X].",
    "dynamic calls": "f(M) -> M:f().",
    "closures": "f() -> fun F(X) -> F(X) end.",
    "receive": "f() -> receive X -> X end.",
    "behavior-changing attributes": "-on_load(f/0). f() -> 1.",
    "send expressions": "f(X) -> X ! 1.",
}


def check(tool, work, feature, options, project, verbose):
    """Require one exact catalog marker with context and preserve existing destinations."""
    args = [tool, *options] + (["--verbose"] if verbose else [])
    args += ["--project", str(work / "project.toml")] if project else [str(work / "sample.erl")]
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=30)
    marker = f"[{feature}] notimpl"
    assert result.returncode == 1 and not result.stdout, (args, result)
    assert result.stderr.count(marker) == 1, (args, result.stderr)
    assert re.search(r"sample\.erl:\d+:\d+:", result.stderr), result.stderr
    assert '[module="sample"]' in result.stderr, result.stderr
    if project:
        assert "audit" in result.stderr, result.stderr
    assert (work / "output/sentinel").read_text(encoding="utf-8") == "preserved"
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
    assert {name for name, owner, status in entries if owner == "compiler" and status == "deferred"} == set(CASES)
    (work / "project.toml").write_text("schema_version=1\n[[targets]]\nname='audit'\nsources=['sample.erl']\n",
                                        encoding="utf-8")
    for feature, body in CASES.items():
        (work / "sample.erl").write_text("-module(sample).\n" + body + "\n", encoding="utf-8")
        for level in ["-O0", "-O2"]:
            options = [level, "--emit", "obj", "--artifact-dir", str(work / "output")]
            for project in [False, True]:
                for verbose in [False, True]:
                    check(tool, work, feature, options, project, verbose)


if __name__ == "__main__":
    main()
