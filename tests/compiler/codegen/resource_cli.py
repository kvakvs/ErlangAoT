"""Exercise public batch admission without constructing resource-exhausting ASTs."""
import pathlib
import subprocess
import sys


def main():
    """Reject oversized positional/project batches before parsing or publication."""
    tool, directory = sys.argv[1:]
    tool = str(pathlib.Path(tool).resolve())
    root = pathlib.Path(directory).resolve()
    root.mkdir(parents=True, exist_ok=True)
    for index in range(1025):
        (root / f"m{index}.erl").write_text(f"-module(m{index}). f() -> 0.\n", encoding="utf-8")
    (root / "project.toml").write_text("schema_version=1\n[[targets]]\nname='limited'\nsources=['m*.erl']\n",
                                        encoding="utf-8")
    for args in [["m0.erl"] * 1025, ["--project", "project.toml"]]:
        result = subprocess.run([tool, "--verbose", "--emit", "obj", "--artifact-dir", "output", *args],
                                cwd=root, capture_output=True, text=True, timeout=30)
        assert result.returncode == 1 and not result.stdout, result
        assert "compilation module count limit exceeded" in result.stderr, result.stderr
        assert "[parse]" not in result.stderr and "[comp]" not in result.stderr
        assert not (root / "output").exists()


if __name__ == "__main__":
    main()
