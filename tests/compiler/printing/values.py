"""Owned term-printing inputs: authored edge values plus every result value of the patternmatch corpora."""
import hashlib
import json
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[3]
FIXTURES = ROOT / 'tests/fixtures/printing'
CORPORA = ROOT / 'tests/fixtures/patternmatch/generated'
GENERATORS = ['values.py', 'regenerate.py', 'oracle.escript']
# Display goldens whose map order follows OTP's internal layout instead of map-key order.
UNORDERED = '?unordered'


class Bits:
    """A bitstring of `length` bits taken MSB-first from `data`, with zero padding."""

    def __init__(self, data, length=None):
        self.data, self.length = data, len(data) * 8 if length is None else length


class Improper:
    """A list whose last tail is `tail` instead of nil."""

    def __init__(self, items, tail):
        self.items, self.tail = items, tail


class Map:
    """A map given as ordered (key, value) pairs."""

    def __init__(self, entries):
        self.entries = entries


def text(characters):
    """An Erlang string: the list of code points."""
    return [ord(c) for c in characters]


def nested(depth):
    """A list nested `depth` times around one atom."""
    value = 'leaf'
    for _ in range(depth):
        value = [value]
    return value


AUTHORED = [
    # Atoms: quoting rules, reserved words and default feature keywords, '@', escapes, Latin-1 and beyond.
    'ok', 'aB9_@', 'Abc', '_x', '9a', '', 'hello world', "it's", 'a\\b', 'a"b', 'a.b', 'a-b',
    'after', 'and', 'andalso', 'band', 'begin', 'bnot', 'bsl', 'catch', 'cond', 'div', 'else', 'end',
    'fun', 'if', 'let', 'maybe', 'not', 'of', 'orelse', 'receive', 'rem', 'try', 'when', 'xor',
    'true', 'false', 'undefined', 'nonode@nohost',
    '\n', '\r\t\v\b\f', '\x00\x01\x1f', '\x1b', '\x7f', '\x80\x9f', '\xa0', 'a\xa0',
    'é', 'aé', 'ßÿ', 'ß', 'Ö', 'aÖ', 'a÷', 'a×', '÷', 'Þ', 'ÿÞ', 'λ', 'aλ', '日本', '😀', 'a😀b', 'Ā',
    # Integers around both small-integer widths and multiprecision values.
    0, 1, -1, 42, -7, 134217727, -134217728, 134217728, 576460752303423487, -576460752303423488,
    576460752303423488, 2**64, -(2**100), 10**60,
    # Floats: fixed/scientific boundaries, exact-integer edges, subnormals and extremes.
    0.0, -0.0, 1.0, -1.0, 0.1, 0.3, 0.5, 2.5, -2.5, 4.35, 3.14159, 100.0, 1000.0, 1234.5, 12340.0,
    0.001, 0.0001, 1.0e-5, 1.5e-7, 0.000123, 1.0e-10, 1.0e10, 1.0e15, 1.0e16, 1.0e21, 1.0e22, 1.0e23,
    123456789.0, 1234567890.0, 12345678900.0, 123456789012.0, 9007199254740991.0, 9007199254740992.0,
    9007199254740994.0, 18014398509481984.0, 1.0e100, -1.0e-100, 5e-324, 2.2250738585072014e-308,
    1.7976931348623157e308, 1 / 3, 2 / 3,
    # Strings and lists: printable bytes, escapes, Latin-1, non-bytes and improper tails.
    [], text('abc'), text('a b'), text('a\nb'), text('a"b'), text('a\\b'), text('\t\r'), text('é'),
    [233, 255, 160], [128], [127], [27], [7], [256], [-1], text('ab') + [1000], [text('ab'), text('cd')],
    [[]], [[], []], Improper(text('ab'), 'c'), Improper([1], 2), Improper([1, 2], 3),
    Improper([text('a')], text('b')), [1, [2, [3, []]]], [(1,), (2,)],
    # Bitstrings: printable and numeric binaries and partial bytes.
    Bits(b''), Bits(b'abc'), Bits(b'a"b'), Bits(b'a\\b'), Bits(b' ~'), Bits(b'\x7f'), Bits(b'abc\x00'),
    Bits(b'\x01\x02\x03'), Bits(b'\xff'), Bits(b'\x80', 1), Bits(b'\xe0', 3), Bits(b'ab\xc0', 18),
    Bits(b'\xff\xe0', 11), Bits(bytes(range(32, 127))),
    # Tuples and ordinary records.
    (), ('a',), (1, 2, 3), ((), ((),)), ('rec', 1, text('x'), Bits(b'y')), ('point', 1.5, -2.0),
    # Maps: map-key order across types, nested keys and both sides of OTP's 32-key flatmap limit.
    Map([]), Map([('a', 1)]), Map([('b', 2), ('a', 1)]), Map([(1, 'int'), (1.0, 'float')]),
    Map([(2, 'two'), (1.5, 'f')]),
    Map([(Bits(b''), 1), (text('l'), 2), ([], 3), (Map([]), 4), ((), 5), ('a', 6), (1, 7)]),
    Map([(Map([('k', 1)]), (1,)), ((1, 2), Map([]))]), Map([(-0.0, 'n'), (0.0, 'p'), (0, 'i')]),
    Map([(i, i * i) for i in range(32)]), Map([(i, -i) for i in range(33)]),
    # Nesting depth and width.
    nested(200), list(range(300)), tuple(range(100)),
]


def children(value):
    """Direct subterms of an authored value."""
    if isinstance(value, (tuple, list)):
        return list(value)
    if isinstance(value, Improper):
        return [*value.items, value.tail]
    if isinstance(value, Map):
        return [term for entry in value.entries for term in entry]
    return []


def has_atom(value):
    """True when an atom occurs anywhere in the value."""
    return isinstance(value, str) or any(has_atom(child) for child in children(value))


def stable_order(value):
    """OTP's internal map order equals map-key order unless atom-table indices or hashing decide it.

    Atom-bearing keys are ordered by atom index, which varies between VM runs; maps above
    32 keys are hash ordered.
    """
    if isinstance(value, Map) and (len(value.entries) > 32 or (
            len(value.entries) > 1 and any(has_atom(key) for key, _ in value.entries))):
        return False
    return all(stable_order(child) for child in children(value))


def parse(encoded, position=0):
    """Decodes a wire value far enough for stable_order: containers, atoms and opaque scalars."""
    if encoded.startswith(('t(', 'c(', 'm('), position):
        kind, position, items = encoded[position], position + 2, []
        while encoded[position] != ')':
            item, position = parse(encoded, position)
            items.append(item)
            position += encoded[position] == ','
        if kind == 'c':
            return Improper(items[:1], items[1]), position + 1
        return (Map(items) if kind == 'm' else tuple(items)), position + 1
    end = position
    while end < len(encoded) and encoded[end] not in ',)':
        end += 1
    token = encoded[position:end]
    return (token if token.startswith('a') else None), end


def cons(items, tail):
    """Right-nested cons cells ending in an encoded tail."""
    for item in reversed(items):
        tail = f'c({wire(item)},{tail})'
    return tail


def wire(value):
    """Encodes one authored value in the owned textual transport (match_wire.hpp)."""
    if isinstance(value, tuple):
        return 't(' + ','.join(map(wire, value)) + ')' if value else 'tuple'
    if isinstance(value, list):
        return cons(value, 'nil')
    if isinstance(value, Improper):
        return cons(value.items, wire(value.tail))
    if isinstance(value, Bits):
        return f'b{value.length}:{value.data.hex()}'
    if isinstance(value, Map):
        return 'm(' + ','.join(f't({wire(k)},{wire(v)})' for k, v in value.entries) + ')'
    if isinstance(value, float):
        return 'f' + struct.pack('>d', value).hex()
    if isinstance(value, int):
        return f'i{value}'
    return 'a' + value.encode().hex()


def corpus_value(directory, line):
    """Maps one expected-result line to a wire value; None for errors without payload and semantic rows."""
    if not line or ' ' in line or '=' in line:
        return None
    if line.startswith('error:'):
        parts = line.split(':', 2)
        return parts[2] if len(parts) == 3 else None
    if directory == 'atoms':
        return 'a' + line
    return 'i' + line if line.lstrip('-').isdigit() else line


def collect():
    """Authored values first, then the sorted unique corpus values not already authored."""
    corpus = set()
    for path in sorted(CORPORA.glob('*/expected.txt')):
        for line in path.read_text(encoding='utf8').splitlines():
            value = corpus_value(path.parent.name, line)
            if value:
                corpus.add(value)
    authored = [wire(value) for value in AUTHORED]
    return authored + sorted(corpus - set(authored))


def escape(data):
    """Display goldens keep printable ASCII; backslash and every other byte are escaped."""
    return ''.join('\\\\' if b == 0x5C else chr(b) if 0x20 <= b < 0x7F else f'\\x{b:02x}' for b in data)


def digest(path):
    """Hashes file bytes with CRLF normalized to LF so checkouts agree across hosts."""
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def golden_files():
    """Every committed file whose hash the manifest records."""
    names = ['values.txt', 'write.txt', 'display.txt', 'display/answer.erl', 'display/client.erl',
             'display/project.toml', 'display/calls.txt', 'display/expected.txt']
    return {name: FIXTURES / name for name in names}


def verify():
    """Rejects goldens whose files or generators changed without an explicit regeneration."""
    manifest = json.loads((FIXTURES / 'manifest.json').read_text(encoding='utf8'))
    assert manifest['schema'] == 1, 'unknown printing manifest schema'
    actual = {name: digest(path) for name, path in golden_files().items()}
    assert manifest['files_sha256'] == actual, 'stale printing goldens; run regenerate.py'
    values = (FIXTURES / 'values.txt').read_text(encoding='utf8').splitlines()
    assert values == collect(),'authored values or corpora changed; run regenerate.py'
    return manifest
