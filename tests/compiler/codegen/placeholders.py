"""Audit catalog-owned CLI capability paths and explicit executable requests."""
import pathlib
import re
import subprocess
import sys

CASES = {
    "pattern matching": "f({X}) -> X.",
    "guards": "f(X) when is_integer(X) -> X.",
    "multiple clauses": "f(X) -> X; f(Y) -> Y.",
    "arithmetic": "f(X) -> X + 1.",
    "bignum expressions": "f() -> 999999999999999999999999999999999.",
    "atom expressions": "f() -> ok.",
    "heap expressions": "f() -> {1}.",
    "dynamic calls": "f(F) -> F(1).",
    "recursive calls": "f() -> f().",
    "closures": "f() -> fun(X) -> X end.",
    "exceptions": "f() -> try 1 catch _:_ -> 2 end.",
    "receive": "f() -> receive X -> X end.",
    "behavior-changing attributes": "-on_load(f/0). f() -> 1.",
    "send expressions": "f(X) -> X ! 1.",
    "expression sequences": "f() -> 1, 2.",
}


def check(tool, work, feature, options, project, verbose):
    """Require one exact catalog marker with context and preserve existing destinations."""
    args = [tool, *options] + (["--verbose"] if verbose else [])
    args += ["--project", str(work / "project.toml")] if project else [str(work / "sample.erl")]
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=30)
    marker = f"[{feature}] notimpl"
    assert result.returncode == 1 and not result.stdout, (args, result)
    assert result.stderr.count(marker) == 1, (args, result.stderr)
    if feature != "executable linking":
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
    (work / "output/sentinel").write_text("preserved", encoding="utf-8")
    catalog = pathlib.Path(catalog_path).read_text(encoding="utf-8")
    entries = re.findall(r'FeatureInfo\{.*?\.name = "([^"]+)".*?\.owner = FeatureOwner::(\w+).*?\}', catalog, re.S)
    assert {name for name, owner in entries if owner == "compiler"} == set(CASES)
    (work / "project.toml").write_text("schema_version=1\n[[targets]]\nname='audit'\nsources=['sample.erl']\n",
                                        encoding="utf-8")
    for feature, body in [*CASES.items(), ("executable linking", "f() -> 42.")]:
        (work / "sample.erl").write_text("-module(sample).\n" + body + "\n", encoding="utf-8")
        for level in ["-O0", "-O2"]:
            options = [level, "--emit", "obj", "--artifact-dir", str(work / "output")]
            if feature == "executable linking":
                options = [level, "--output", str(work / "output/sentinel")]
            for project in [False, True]:
                for verbose in [False, True]:
                    check(tool, work, feature, options, project, verbose)


if __name__ == "__main__":
    main()
