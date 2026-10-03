"""Verify program fixture goldens offline and compile each project, expecting today's diagnostics."""
import argparse
import pathlib
import shutil
import subprocess
import sys
import fixtures


def compile_fixture(tool, name, work):
    """Compiles a disposable copy of the fixture project; returns exit status and normalized stderr."""
    root = work / name
    shutil.rmtree(root, ignore_errors=True)
    shutil.copytree(fixtures.FIXTURES / name, root, ignore=shutil.ignore_patterns('expected', 'compile.txt'))
    result = subprocess.run([tool, '--project', 'project.toml'], cwd=root, capture_output=True, timeout=120,
                            check=False)
    assert result.stdout == b'', f'{name}: unexpected stdout {result.stdout!r}'
    stderr = result.stderr.decode('utf8').replace('\r\n', '\n').replace('\\', '/')
    return result.returncode, stderr.replace(root.resolve().as_posix(), '<fixture>')


def expected(name):
    """Reads compile.txt: an 'exit N' line followed by the exact expected stderr."""
    text = (fixtures.FIXTURES / name / 'compile.txt').read_text(encoding='utf8').replace('\r\n', '\n')
    status, _, stderr = text.partition('\n')
    assert status.startswith('exit '), name
    return int(status.removeprefix('exit ')), stderr


def check(tool, name, work, update):
    """Checks one fixture's golden hashes and compile outcome; returns a failure message or None."""
    manifest, stdout = fixtures.verify(name)
    assert stdout and manifest['exit_status'] in range(256), name
    status, stderr = compile_fixture(tool, name, work)
    if update:
        (fixtures.FIXTURES / name / 'compile.txt').write_bytes(f'exit {status}\n{stderr}'.encode())
        return None
    want_status, want_stderr = expected(name)
    if (status, stderr) != (want_status, want_stderr):
        return f'{name}: expected exit {want_status}\n{want_stderr}got exit {status}\n{stderr}'
    print(f'{name}: golden exit {manifest["exit_status"]}, compile exit {status}, '
          f'{stderr.count(chr(10))} diagnostic lines')
    return None


def main():
    """Runs every fixture; --update-diagnostics rewrites compile.txt after a reviewed feature change."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', type=pathlib.Path)
    parser.add_argument('work', type=pathlib.Path)
    parser.add_argument('--update-diagnostics', action='store_true')
    options = parser.parse_args()
    names = fixtures.names()
    assert len(names) >= 4, names
    options.work.mkdir(parents=True, exist_ok=True)
    failures = [failure for name in names
                if (failure := check(options.tool, name, options.work, options.update_diagnostics))]
    for failure in failures:
        print(failure, file=sys.stderr)
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
