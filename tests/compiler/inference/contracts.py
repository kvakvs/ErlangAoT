"""Specification contradictions (docs/semantic.md#inference): each fixture of tests/fixtures/inference/contracts lists
the errors `--print-types` must report (`%% error: Text`) and the other modules it is compiled with (`%% with:
file`); a fixture without error lines must compile without one.

    contracts.py <clau> <fixture directory>
"""
import pathlib
import re
import subprocess
import sys

ERROR = re.compile(r'^%% error: (.+)$')
WITH = re.compile(r'^%% with: (.+)$')


def check(tool, path):
    """Problems with one fixture's diagnostics."""
    lines = path.read_text(encoding='utf-8').splitlines()
    errors = [match.group(1) for match in map(ERROR.match, lines) if match]
    others = [path.parent / match.group(1) for match in map(WITH.match, lines) if match]
    result = subprocess.run([tool, '--print-types', path.name, *(other.name for other in others)], cwd=path.parent,
                            capture_output=True, text=True, encoding='utf-8', timeout=120, check=False)
    if not errors:
        bad = [line for line in result.stderr.splitlines() if 'error:' in line]
        return [f'{path.name}: unexpected {line}' for line in bad] or ([] if result.returncode == 0 else
                                                                         [f'{path.name}: exit {result.returncode}'])
    problems = [] if result.returncode == 1 else [f'{path.name}: exit {result.returncode}, wanted 1']
    reported = [line for line in result.stderr.splitlines() if 'specification' in line]
    problems += [f'{path.name}: missing `{error}`' for error in errors
                 if not any(line.endswith(error) for line in reported)]
    problems += [f'{path.name}: unexpected {line}' for line in reported
                 if not any(line.endswith(error) for error in errors)]
    return problems


def main():
    tool, directory = sys.argv[1], pathlib.Path(sys.argv[2])
    owners = {match.group(1) for path in directory.glob('*.erl')
              for match in map(WITH.match, path.read_text(encoding='utf-8').splitlines()) if match}
    fixtures = sorted(path for path in directory.glob('*.erl') if path.name not in owners)
    problems = [problem for path in fixtures for problem in check(tool, path)]
    for problem in problems:
        print(problem)
    print(f'contracts: {len(fixtures)} fixtures, {len(problems)} problem(s)')
    sys.exit(1 if problems else 0)


if __name__ == '__main__':
    main()
