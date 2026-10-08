"""Type inference expectations: each function of a fixture module carries `%% expect: Signature` (what --print-types
should infer) and, while inference falls short of it, `%% today: Signature` (what it infers now). The check compares
--print-types with `today` when present, else with `expect`, and fails when a `today` line went stale.

    expectations.py <clau> <fixture.erl>             check
    expectations.py <clau> <fixture.erl> --record    rewrite the today lines from the current output
"""
import pathlib
import re
import subprocess
import sys

EXPECT = re.compile(r'^%% expect: (.+)$')
TODAY = re.compile(r'^%% today: (.+)$')
INFERRED = re.compile(r'^%% inferred: (.+)$')


def key(signature):
    """`name/arity` of a signature `name(Inputs) -> Result`; commas inside nested types do not count."""
    name, rest = signature.split('(', 1)
    depth, count, empty = 0, 1, True
    for character in rest:
        if character == ')' and depth == 0:
            break
        depth += character in '([{<'
        depth -= character in ')]}>'
        count += character == ',' and depth == 0
        empty = empty and character.isspace()
    return f'{name}/{0 if empty else count}'


def expectations(path):
    """The expect and today signatures of the fixture, by function key, in source order."""
    table, pending = {}, {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if match := EXPECT.match(line):
            pending = {'expect': match.group(1)}
            table[key(match.group(1))] = pending
        elif match := TODAY.match(line):
            pending['today'] = match.group(1)
    return table


def inferred(tool, path):
    """The inferred signatures --print-types reports for the fixture's own module, by function key."""
    result = subprocess.run([tool, '--print-types', str(path)], capture_output=True, text=True, encoding='utf-8',
                            timeout=120, check=False)
    if result.returncode != 0:
        sys.exit(f'{path.name}: --print-types failed:\n{result.stderr}')
    own = result.stdout.split('\n%% module ', 1)[0]
    return {key(match.group(1)): match.group(1) for match in map(INFERRED.match, own.splitlines()) if match}


def check(table, actual):
    """Problems with the fixture's expectations against the current output, and how many reach their type."""
    problems = [f'{name}: no expectation' for name in actual if name not in table]
    reached = 0
    for name, entry in table.items():
        found = actual.get(name)
        wanted = entry.get('today', entry['expect'])
        if found is None:
            problems.append(f'{name}: not in the --print-types output')
        elif 'today' in entry and found == entry['expect']:
            problems.append(f'{name}: inference now finds the expected `{found}`; remove its today line')
        elif found != wanted:
            problems.append(f'{name}: inferred `{found}`, wanted `{wanted}`')
        reached += found == entry['expect']
    return problems, reached


def record(path, actual):
    """Rewrite the today lines: one after each expect line whose function's inferred signature differs."""
    lines, output = path.read_text(encoding='utf-8').splitlines(), []
    for line in lines:
        if TODAY.match(line):
            continue
        output.append(line)
        if match := EXPECT.match(line):
            found = actual.get(key(match.group(1)))
            if found is not None and found != match.group(1):
                output.append(f'%% today: {found}')
    path.write_bytes(('\n'.join(output) + '\n').encode('utf-8'))


def main():
    tool, path = sys.argv[1], pathlib.Path(sys.argv[2])
    actual = inferred(tool, path)
    if sys.argv[3:] == ['--record']:
        record(path, actual)
        return
    table = expectations(path)
    problems, reached = check(table, actual)
    for problem in problems:
        print(problem)
    print(f'{path.stem}: {len(table)} functions, {reached} at their expected type, {len(problems)} problem(s)')
    sys.exit(1 if problems else 0)


if __name__ == '__main__':
    main()
