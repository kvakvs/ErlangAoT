"""Shared lookup and hashing for project-owned program fixtures."""
import hashlib
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
FIXTURES = ROOT / 'tests/fixtures/programs'
GENERATORS = ['regenerate.py', 'fixtures.py', 'oracle.escript']


def digest(path):
    """Hashes file bytes with CRLF normalized to LF so checkouts agree across hosts."""
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def names():
    """Lists fixture directories, each identified by its fixture.json."""
    return sorted(path.parent.name for path in FIXTURES.glob('*/fixture.json'))


def spec(name):
    """Loads the entry module and argv of one fixture."""
    data = json.loads((FIXTURES / name / 'fixture.json').read_text(encoding='utf8'))
    assert data['schema'] == 1 and isinstance(data['entry'], str), name
    assert all(isinstance(arg, str) for arg in data['args']), name
    return data


def inputs(name):
    """Hashes every input that determines the golden: manifest, spec and Erlang sources."""
    root = FIXTURES / name
    paths = [root / 'project.toml', root / 'fixture.json', *sorted((root / 'src').rglob('*.erl'))]
    return {path.relative_to(root).as_posix(): digest(path) for path in paths}


def golden(name):
    """Loads the committed golden manifest and expected stdout of one fixture."""
    root = FIXTURES / name / 'expected'
    manifest = json.loads((root / 'golden.json').read_text(encoding='utf8'))
    return manifest, (root / 'stdout.txt').read_bytes().replace(b'\r\n', b'\n')


def verify(name):
    """Rejects a golden whose recorded inputs or stdout no longer match the files."""
    manifest, stdout = golden(name)
    assert manifest['schema'] == 1 and manifest['fixture'] == name, name
    assert manifest['inputs'] == inputs(name), f'Stale program golden inputs: {name}'
    assert manifest['stdout_sha256'] == hashlib.sha256(stdout).hexdigest(), f'Stale stdout: {name}'
    return manifest, stdout
