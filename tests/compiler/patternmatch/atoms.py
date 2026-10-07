"""Execute locally authored atom and boolean fragments through the public CLI and native runtime."""
import pathlib
import subprocess
import sys
from stored import load
from evidence import run
from matrix import combinations


def failed_batch(tool, work):
    """An unsupported later source must leave no earlier atom artifact, followed by a successful retry."""
    bad = work / "bad.erl"
    bad.write_text("-module(bad). value(X) -> X#_.a.\n", encoding="utf-8")
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
    for level, extra, _, project in combinations():
        name = level + ("-off" if extra else "-on")
        run([cmake, f"-DTOOL={tool}", f"-DOPTIMIZATION={level}", f"-DEXTRA_OPTIONS={extra}",
             f"-DPROJECT_MODE={project}", f"-DSOURCE_ROOT={source.as_posix()}",
             f"-DTEST_DIR={(work / name).as_posix()}", f"-DINPUT_ROOT={work.as_posix()}",
             f"-DHOST_SETTINGS={settings}", f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}",
             "-P", str(source / "tests/compiler/codegen/atoms.cmake")])
    print("OTP atom spellings/booleans match all four native policies; ownership, limits, rollback and retry pass.")


if __name__ == "__main__":
    main()
