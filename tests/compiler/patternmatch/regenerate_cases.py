"""Observe local semantic and projection fragments during explicit OTP refresh."""
from evidence import run


def native(source, escript, work):
    """Obtain actual OTP execution for the plain-integer consumer format."""
    expected = run([escript, str(source / 'tests/compiler/codegen/execution_oracle.escript'), str(work)])
    (work / 'expected.txt').write_bytes(expected.replace('\r\n', '\n').encode())
    return expected


def generate(source, escript, name, work, evidence):
    """Keep local inputs fixed while refreshing independent acceptance and execution observations."""
    if name in ['baseline', 'differential']:
        output = native(source, escript, work)
        assert len(output.splitlines()) == evidence['calls']
        return evidence, '../codegen/execution_oracle.escript'
    oracle_name = name + '.escript'
    oracle = run([escript, str(source / 'tests/compiler/patternmatch' / oracle_name), str(work)])
    evidence['oracle'] = oracle
    if name == 'bindings':
        native(source, escript, work)
    else:
        (work / 'expected.txt').write_bytes(oracle.replace('\r\n', '\n').encode())
    return evidence, oracle_name
