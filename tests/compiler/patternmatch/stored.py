"""Load project-owned oracle data without consulting an OTP checkout or runtime."""
import json
import pathlib
from evidence import digest


def load(source, name, work):
    """Verify every committed input/result before copying it into the disposable native work area."""
    root = source / 'tests/fixtures/patternmatch/generated' / name
    manifest = json.loads((root / 'manifest.json').read_text(encoding='utf8'))
    assert manifest['schema'] == 1 and manifest['corpus'] == name
    for filename, expected in manifest['files'].items():
        assert filename == pathlib.Path(filename).name, filename
        path = root / filename
        assert digest(path) == expected, f'Stale stored oracle fixture: {name}/{filename}'
    work.mkdir(parents=True, exist_ok=True)
    for filename in manifest['files']:
        (work / filename).write_bytes((root / filename).read_bytes().replace(b'\r\n', b'\n'))
    return manifest['evidence']
