""".abstr files as compiler inputs: linked programs, a project target and invalid forms (added for parse transforms)."""
import pathlib
import shutil
import subprocess
import sys

PROGRAMS = ["abstr_program.abstr", "abstr_program.erl", "crafted.abstr", "lines.abstr"]


def run(command, cwd):
    """Run a command, capturing text output."""
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8", timeout=300,
                          check=False)


def linked(tool, fixtures, work, source):
    """Link one input into an executable, run it and compare its output with the expected one."""
    stem = pathlib.Path(source).stem
    executable = work / (source.replace(".", "_") + ".exe")
    built = run([str(tool), "-o", str(executable), source], fixtures)
    if built.returncode != 0:
        return [f"{source}: compile failed\n{built.stderr}"]
    result = run([str(executable)], work)
    expected = (fixtures / f"{stem}.stdout").read_text(encoding="utf-8")
    if result.returncode != 0 or result.stdout.replace("\r\n", "\n") != expected:
        return [f"{source}: exit {result.returncode}, stdout:\n{result.stdout}"]
    return []


def project(tool, fixtures, work):
    """A project target lists a .abstr source."""
    root = work / "project"
    root.mkdir(exist_ok=True)
    shutil.copy(fixtures / "abstr_program.abstr", root / "abstr_program.abstr")
    (root / "project.toml").write_bytes(b"schema_version = 1\n[[targets]]\nname = 'program'\n"
                                        b"sources = ['abstr_program.abstr']\noutput = 'program.exe'\n")
    built = run([str(tool), "--project", "project.toml"], root)
    if built.returncode != 0:
        return [f"project: compile failed\n{built.stderr}"]
    result = run([str(root / "program.exe")], root)
    expected = (fixtures / "abstr_program.stdout").read_text(encoding="utf-8")
    return [] if result.stdout.replace("\r\n", "\n") == expected else [f"project: stdout:\n{result.stdout}"]


def invalid(tool, fixtures):
    """Malformed forms are diagnosed at their annotations and fail the module."""
    result = run([str(tool), "bad.abstr"], fixtures)
    expected = (fixtures / "bad.stderr").read_text(encoding="utf-8")
    if result.returncode != 1 or result.stderr.replace("\r\n", "\n") != expected:
        return [f"bad.abstr: exit {result.returncode}, stderr:\n{result.stderr}"]
    return []


def main():
    """inputs.py CLAU FIXTURES WORK"""
    tool, fixtures, work = (pathlib.Path(argument).resolve() for argument in sys.argv[1:])
    work.mkdir(parents=True, exist_ok=True)
    failures = invalid(tool, fixtures) + project(tool, fixtures, work)
    for source in PROGRAMS:
        failures += linked(tool, fixtures, work, source)
    if failures:
        print("\n".join(failures), file=sys.stderr)
        sys.exit(1)
    print("transforms_inputs: ok")


if __name__ == "__main__":
    main()
