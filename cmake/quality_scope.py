"""Select project files for scoped quality checks from git changes and Ninja header dependencies."""
import argparse
import json
import os
import subprocess

PRODUCTION = ("compiler/", "runtime/", "abi/")
CPP = (".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh", ".hxx")


def norm(path):
    """Compare paths case-insensitively where the host does, independent of separators."""
    return os.path.normcase(os.path.normpath(path))


def git_lines(root, *args):
    """Run git in the repository and return its nonempty output lines."""
    result = subprocess.run(["git", "-C", root, *args], capture_output=True, text=True, encoding="utf-8")
    if result.returncode:
        raise SystemExit(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return [line for line in result.stdout.splitlines() if line]


def changed_files(root, base):
    """Existing files changed since base (including the working tree) plus untracked files."""
    tracked = git_lines(root, "diff", "--name-only", base, "--")
    untracked = git_lines(root, "ls-files", "--others", "--exclude-standard")
    return sorted(path for path in set(tracked + untracked) if os.path.isfile(os.path.join(root, path)))


def affects_everything(path):
    """Analyzer settings and production build rules can change every translation unit."""
    if path in (".clang-tidy", "CMakeLists.txt") or path.startswith("cmake/"):
        return True
    return path.startswith(PRODUCTION) and path.endswith("CMakeLists.txt")


def production_units(root, build):
    """Map each production translation unit to its object path, as both appear in the database."""
    with open(os.path.join(build, "compile_commands.json"), encoding="utf-8") as stream:
        commands = json.load(stream)
    units = {}
    for entry in commands:
        source = os.path.join(entry["directory"], entry["file"])
        relative = os.path.relpath(source, root).replace(os.sep, "/")
        if relative.startswith(PRODUCTION):
            units[relative] = norm(os.path.join(entry["directory"], entry.get("output", "")))
    return units


def header_users(build, ninja, headers):
    """Objects whose recorded dependencies include a changed header; None when Ninja data is missing."""
    if not ninja or not os.path.exists(os.path.join(build, ".ninja_deps")):
        return None
    result = subprocess.run([ninja, "-C", build, "-t", "deps"], capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    if result.returncode:
        return None
    wanted, users, current = {norm(header) for header in headers}, set(), None
    for line in result.stdout.splitlines():
        if line and not line[0].isspace():
            current = norm(os.path.join(build, line.split(": #deps", 1)[0]))
        elif current and norm(os.path.join(build, line.strip())) in wanted:
            users.add(current)
    return users


def tidy_scope(root, build, ninja, changed):
    """Changed units plus units including changed production headers, or every unit when unknown."""
    units = production_units(root, build)
    if any(affects_everything(path) for path in changed):
        return sorted(units), "analyzer or build configuration changed"
    selected = {path for path in changed if path in units}
    headers = [os.path.join(root, path) for path in changed
               if path.startswith(PRODUCTION) and path.endswith(CPP) and path not in units]
    if headers:
        users = header_users(build, ninja, headers)
        if users is None:
            return sorted(units), "header dependencies unavailable"
        selected |= {path for path, output in units.items() if output in users}
    return sorted(selected), "changed files and their header dependents"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=["tidy", "lizard"], required=True)
    parser.add_argument("--root", required=True)
    parser.add_argument("--build")
    parser.add_argument("--ninja", default="")
    parser.add_argument("--base", default="HEAD")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    changed = changed_files(args.root, args.base)
    if args.kind == "lizard":
        files = [path for path in changed if path.startswith(PRODUCTION) and path.endswith(CPP)]
        reason = "changed production C++ files"
    else:
        files, reason = tidy_scope(args.root, args.build, args.ninja, changed)
    with open(args.output, "w", encoding="utf-8", newline="\n") as stream:
        stream.write("".join(path + "\n" for path in files))
    print(f"Quality scope ({args.kind}, base {args.base}): {len(files)} file(s); {reason}")


if __name__ == "__main__":
    main()
