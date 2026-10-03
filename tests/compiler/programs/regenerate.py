"""Explicitly regenerate, or check, program fixture goldens with an installed OTP."""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import fixtures

HERE = pathlib.Path(__file__).resolve().parent


def reference():
    """Reads the pinned maint-29 revision recorded alongside the oracle version."""
    text = (fixtures.ROOT / 'references/otp-pin.cmake').read_text(encoding='utf8')
    branch = re.search(r'REFERENCE_BRANCH "([^"]+)"', text).group(1)
    return {'branch': branch, 'revision': re.search(r'REFERENCE_REVISION "([0-9a-f]{40})"', text).group(1)}


def observe(escript, name, work):
    """Runs the fixture entry under OTP and returns stdout, exit status and oracle version."""
    spec = fixtures.spec(name)
    version = work / f'{name}.version'
    command = [escript, str(HERE / 'oracle.escript'), str(version), str(fixtures.FIXTURES / name / 'src'),
               spec['entry'], *spec['args']]
    result = subprocess.run(command, cwd=work, capture_output=True, timeout=300, check=False)
    if result.returncode == 125 or not version.exists():
        sys.exit(f'{name}: oracle failed: {result.stderr.decode(errors="replace")}')
    return result.stdout.replace(b'\r\n', b'\n'), result.returncode, version.read_text().split()


def manifest(name, stdout, status, version):
    """Builds the golden manifest that ties expected behavior to its inputs and oracle."""
    return {
        'schema': 1,
        'fixture': name,
        'oracle_version': version,
        'reference': reference(),
        'contract': 'main/1 argv strings; return exits 0; halt/1 sets status; uncaught exception exits 1',
        'generator_sha256': {file: fixtures.digest(HERE / file) for file in fixtures.GENERATORS},
        'inputs': fixtures.inputs(name),
        'exit_status': status,
        'stdout_sha256': hashlib.sha256(stdout).hexdigest(),
    }


def refresh(escript, name, work, check):
    """Writes one golden, or in check mode reports whether OTP still reproduces it."""
    stdout, status, version = observe(escript, name, work)
    expected = manifest(name, stdout, status, version)
    root = fixtures.FIXTURES / name / 'expected'
    if check:
        committed, committed_stdout = fixtures.golden(name)
        same = committed == expected and committed_stdout == stdout
        print(f'{name}: {"reproduced" if same else "DRIFT"} (exit {status}, {len(stdout)} bytes)')
        return same
    root.mkdir(exist_ok=True)
    (root / 'stdout.txt').write_bytes(stdout)
    (root / 'golden.json').write_bytes((json.dumps(expected, indent=2) + '\n').encode())
    print(f'{name}: wrote exit {status}, {len(stdout)} bytes')
    return True


def main():
    """Parses options and refreshes or checks every selected fixture."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--escript', required=True, help='OTP 29 escript executable')
    parser.add_argument('--fixture', action='append', choices=fixtures.names(), help='limit to one fixture')
    parser.add_argument('--check', action='store_true', help='compare with committed goldens, write nothing')
    options = parser.parse_args()
    work_root = fixtures.ROOT / 'build'
    work_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='programs-oracle-', dir=work_root) as work:
        results = [refresh(options.escript, name, pathlib.Path(work), options.check)
                   for name in options.fixture or fixtures.names()]
    sys.exit(0 if all(results) else 1)


if __name__ == '__main__':
    main()
