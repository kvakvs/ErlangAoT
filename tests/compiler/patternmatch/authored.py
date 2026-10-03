"""Stage local source fragments and fixed inputs for an explicit OTP observation refresh."""
import json
import pathlib
from evidence import digest


def fragments(source, name):
    """Keep authored Erlang sources separate from generated oracle observations."""
    return source / 'tests/fixtures/patternmatch/fragments' / name


def fixture_path(source, name, filename):
    """Resolve source fixtures locally and oracle data in the retained observation directory."""
    root = fragments(source, name) if pathlib.Path(filename).suffix in ['.erl', '.hrl'] else (
        source / 'tests/fixtures/patternmatch/generated' / name)
    return root / filename


def stage(source, name, work):
    """Copy fixed authored inputs without reading OTP source or existing expected results."""
    root = fragments(source, name)
    record = json.loads((root / 'corpus.json').read_text(encoding='utf8'))
    for filename in record['inputs']:
        path = fixture_path(source, name, filename)
        (work / filename).write_bytes(path.read_bytes().replace(b'\r\n', b'\n'))
    evidence = record['evidence']
    for row in evidence.get('cases', []):
        row['sha256'] = digest(work / (row['name'] + '.erl'))
    evidence['source_origin'] = 'locally authored fragments; OTP supplies observations only'
    evidence['source_sha256'] = {path.name: digest(path) for path in sorted(root.glob('*'))
                                 if path.suffix in ['.erl', '.hrl']}
    return evidence
