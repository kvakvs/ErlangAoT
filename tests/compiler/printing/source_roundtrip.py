"""Source printing round trip: --print-source of every parseable fixture module parses back to the same syntax tree,
and printing the printed text again changes nothing."""
import concurrent.futures
import pathlib
import re
import subprocess
import sys

# Directories whose .erl files are checked, relative to the repository root.
SOURCES = ['tests/fixtures/parser', 'tests/fixtures/executables', 'tests/fixtures/programs', 'library']


def run(tool, action, path):
    """Runs one printing action; returns its exit status and stdout."""
    result = subprocess.run([tool, action, str(path)], capture_output=True, text=True, encoding='utf-8',
                            errors='replace', timeout=60, check=False)
    return result.returncode, result.stdout


def tree(tool, path):
    """The syntax tree of a file without the -file form the preprocessor adds for its name."""
    status, text = run(tool, '--print-ast', path)
    return status, re.sub(r'form\[0\]=\(FileAttribute name="[^"]*" line=1\)', 'FILE', text, count=1)


def check(tool, work, source):
    """Returns a problem description for one source, or None when it round-trips (or does not parse at all)."""
    status, original = tree(tool, source)
    if status != 0:
        return None
    printed_status, printed = run(tool, '--print-source', source)
    if printed_status != 0:
        return f'{source}: --print-source failed on a module that parses'
    copy = work / f'{abs(hash(str(source)))}.erl'
    copy.write_text(printed, encoding='utf-8')
    if tree(tool, copy) != (0, original):
        return f'{source}: the printed source parses to another tree'
    if run(tool, '--print-source', copy) != (0, printed):
        return f'{source}: printing the printed source changed it'
    return None


def main():
    tool, root, work = sys.argv[1], pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3])
    work.mkdir(parents=True, exist_ok=True)
    sources = sorted(path for directory in SOURCES for path in (root / directory).rglob('*.erl'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        problems = [problem for problem in pool.map(lambda source: check(tool, work, source), sources) if problem]
    for problem in problems:
        print(problem)
    print(f'{len(sources)} modules, {len(problems)} problem(s)')
    sys.exit(1 if problems else 0)


if __name__ == '__main__':
    main()
