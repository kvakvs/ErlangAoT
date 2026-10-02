"""Generate retained acceptance and projection corpora only during explicit OTP refresh."""
import importlib.util
import json
from evidence import baseline, run


def native(source, escript, work):
    """Use actual OTP execution for the historical plain-integer consumer format."""
    expected = run([escript, str(source / 'tests/compiler/codegen/execution_oracle.escript'), str(work)])
    (work / 'expected.txt').write_bytes(expected.replace('\r\n', '\n').encode())
    return expected


def generate(source, otp, escript, name, work):
    """Preserve existing semantic cases and complete helpers without requiring OTP in routine tests."""
    if name == 'baseline':
        baseline(otp, work)
        native(source, escript, work)
        return {'calls': 3, 'helpers': json.loads((work / 'helpers.json').read_text(encoding='utf8'))}, '../codegen/execution_oracle.escript'
    if name == 'differential':
        path = source / 'tests/compiler/codegen/differential.py'
        spec = importlib.util.spec_from_file_location('projection_generator', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        expected = module.generate(source, work)
        assert native(source, escript, work) == expected
        return {'calls': len(expected.splitlines()), 'seed': 290041}, '../codegen/execution_oracle.escript'
    if name == 'bindings':
        from bindings import source_cases, helpers
        rows, terms = source_cases(source, work)
        helper_record = helpers(otp, work)
        terms += ['{bindings_otp, accepted, none}.', '{answer, accepted, none}.', '{client, accepted, none}.']
        (work / 'bindings.term').write_bytes(('\n'.join(terms) + '\n').encode())
        oracle = run([escript, str(source / 'tests/compiler/patternmatch/bindings.escript'), str(work)])
        rows.append({'name': 'bindings_otp', 'diagnostic': '', 'capability': ''})
        native(source, escript, work)
    else:
        from patterns import cases
        rows, helper_record = cases(source, otp, work)
        oracle = run([escript, str(source / 'tests/compiler/patternmatch/patterns.escript'), str(work)])
        (work / 'expected.txt').write_bytes(oracle.replace('\r\n', '\n').encode())
    return {'cases': rows, 'helpers': helper_record, 'oracle': oracle}, name + '.escript'
