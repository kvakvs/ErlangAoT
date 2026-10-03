"""Explicitly regenerate, or check, term-printing goldens with an installed OTP 29."""
import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import values

HERE = pathlib.Path(__file__).resolve().parent
# A few values also go through the remote client and the two-argument display sequence.
CLIENT_CALLS = 12
PAIRS = [('ok', 'Abc'), ((1, 2), 2.5), ([], values.Map([(1, 'a')]))]


def reference():
    """Reads the pinned maint-29 revision recorded alongside the oracle version."""
    text = (values.ROOT / 'references/otp-pin.cmake').read_text(encoding='utf8')
    branch = re.search(r'REFERENCE_BRANCH "([^"]+)"', text).group(1)
    return {'branch': branch, 'revision': re.search(r'REFERENCE_REVISION "([0-9a-f]{40})"', text).group(1)}


def oracle(escript, work, *arguments):
    """Runs the OTP oracle once and returns its stdout and version lines."""
    version = work / 'version.txt'
    result = subprocess.run([escript, str(HERE / 'oracle.escript'), str(version), *arguments], cwd=work,
                            capture_output=True, timeout=600, check=False)
    if result.returncode != 0:
        sys.exit(f'oracle failed: {result.stderr.decode(errors="replace")}')
    return result.stdout.replace(b'\r\n', b'\n'), version.read_text().split()


def renderings(escript, work, inputs):
    """Observes ~w and emulator text for every input value."""
    (work / 'values.txt').write_text('\n'.join(inputs) + '\n', encoding='utf8')
    _, version = oracle(escript, work, 'values', str(work / 'values.txt'), str(work / 'observed.txt'))
    rows = [line.split('\t') for line in (work / 'observed.txt').read_text(encoding='ascii').splitlines()]
    assert len(rows) == len(inputs), 'oracle skipped values'
    write = [bytes.fromhex(row[0]) for row in rows]
    stable = [values.stable_order(values.parse(value)[0]) for value in inputs]
    display = [bytes.fromhex(row[1]) if keep else None for row, keep in zip(rows, stable)]
    assert all(b'\n' not in line and b'\r' not in line for line in write), '~w text spans lines'
    return write, display, version


def display_calls(inputs, display):
    """Native calls use authored values with ordered, plain-ASCII display text, so CTest compares it as text."""
    count = len(values.AUTHORED)
    safe = [value for value, text in zip(inputs[:count], display[:count])
            if text is not None and values.escape(text) == text.decode('latin-1')]
    calls = [f'answer show 1 {value}' for value in safe]
    calls += [f'client show 1 {value}' for value in safe[:CLIENT_CALLS]]
    calls += [f'answer pair 2 {values.wire(a)} {values.wire(b)}' for a, b in PAIRS]
    return calls


def generate(escript, work, target):
    """Writes every generated golden below `target` and returns the manifest."""
    inputs = values.collect()
    write, display, version = renderings(escript, work, inputs)
    (target / 'display').mkdir(parents=True, exist_ok=True)
    (target / 'values.txt').write_bytes(('\n'.join(inputs) + '\n').encode())
    (target / 'write.txt').write_bytes(b''.join(line + b'\n' for line in write))
    shown = [values.UNORDERED if line is None else values.escape(line) for line in display]
    (target / 'display.txt').write_bytes(''.join(line + '\n' for line in shown).encode())
    calls = display_calls(inputs, display)
    (target / 'display/calls.txt').write_bytes(('\n'.join(calls) + '\n').encode())
    stdout, _ = oracle(escript, work, 'calls', str(values.FIXTURES / 'display'), str(target / 'display/calls.txt'))
    (target / 'display/expected.txt').write_bytes(stdout)
    return {
        'schema': 1,
        'oracle_version': version,
        'reference': reference(),
        'generator_sha256': {file: values.digest(HERE / file) for file in values.GENERATORS},
        'counts': {'values': len(inputs), 'authored': len(values.AUTHORED), 'calls': len(calls),
                   'display_unordered': display.count(None)},
    }


def files(target, manifest):
    """Adds the hashes of all golden files as found below `target`."""
    names = values.golden_files()
    manifest['files_sha256'] = {name: values.digest(target / path.relative_to(values.FIXTURES))
                                for name, path in names.items()}
    return manifest


def main():
    """Regenerates the printing goldens, or checks that OTP still reproduces them."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--escript', required=True, help='OTP 29 escript executable')
    parser.add_argument('--check', action='store_true', help='compare with committed goldens, write nothing')
    options = parser.parse_args()
    work_root = values.ROOT / 'build'
    work_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='printing-oracle-', dir=work_root) as work:
        work = pathlib.Path(work)
        target = work / 'golden' if options.check else values.FIXTURES
        for name in ['answer.erl', 'client.erl', 'project.toml']:
            (target / 'display').mkdir(parents=True, exist_ok=True)
            if options.check:
                (target / 'display' / name).write_bytes((values.FIXTURES / 'display' / name).read_bytes())
        manifest = files(target, generate(options.escript, work, target))
        if options.check:
            committed = json.loads((values.FIXTURES / 'manifest.json').read_text(encoding='utf8'))
            same = committed == manifest
            print('printing goldens: ' + ('reproduced' if same else 'DRIFT'))
            sys.exit(0 if same else 1)
        (target / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
        print(f"printing goldens: wrote {manifest['counts']}")


if __name__ == '__main__':
    main()
