"""Audit every pinned guard signature through owned native results and explicit dependency gates."""
import csv
import itertools
import json
import pathlib
import re
import sys
from evidence import digest, run
from immediate import native
from services import write_calls
from stored import load
from bindings import compile_case

BIG = 1 << 100
BIN = {'bits': '0102', 'length': 16}
BITS = {'bits': 'a0', 'length': 3}
MAP = {'map': [('a', 1), (1, 'true')]}
VALUES = [-BIG, -1, 0, 1, BIG, 1.0, 'a', 'true', 'false', 'nil', ('r', 1), [1],
          {'cons': [1, 'a']}, MAP, BIN, BITS]
BLOCKED = {('node', 0): 'F07/F22/F26', ('node', 1): 'F07/F22/F26',
           ('self', 0): 'F07/F22/F26', ('is_record', 1): 'F17 native records'}


def arguments(name, arity):
    """Cover admitted kinds, bad arguments and useful exact boundaries without unbounded oracle shifts."""
    if name == 'is_record':
        return [[v] for v in VALUES + [('r',), ('r', 1, 2), ('s', 1), ()]]
    if arity == 1:
        return [[v] for v in VALUES]
    if name == 'is_integer' and arity == 3:
        return [[v, lo, hi] for v in [-BIG, -1, 0, 1, BIG, 1.0, 'a']
                for lo, hi in [(-BIG, BIG), (-1, 1), (1, 1), (1, -1), ('a', 1), (0, 1.0)]]
    if name == 'binary_part':
        pairs = [(BIN, 0, 2), (BIN, 2, -2), (BIN, 2, 0), (BIN, -1, 1),
                 (BIN, 0, 3), (BITS, 0, 0), ('a', 0, 0), (BIN, 'a', 1), (BIN, BIG, 1)]
        return [[v, (p, n)] if arity == 2 else [v, p, n] for v, p, n in pairs]
    if name in ['map_get', 'is_map_key']:
        return [[k, m] for k in ['a', 'missing', 1, 1.0, ()] for m in [MAP, {'map': []}, 'a']]
    if name == 'element':
        return [[n, v] for n in [-1, 0, 1, 2, 3, BIG, 'a', 1.0] for v in [('a', 1), (), 'a']]
    if name == 'is_function':
        return [[v, n] for v in [0, 'a', (), MAP] for n in [-BIG, -1, 0, 1, BIG, 'a', 1.0]]
    if name in ['and', 'or', 'xor']:
        return [list(p) for p in itertools.product(['true', 'false', 'a', 0], repeat=2)]
    if name in ['==', '/=', '=<', '<', '>=', '>', '=:=', '=/=', 'min', 'max']:
        return [[v, w] for v, w in zip(VALUES, reversed(VALUES))] + [[v, v] for v in VALUES] + [[1, 1.0], [1.0, 1]]
    if name in ['bsl', 'bsr']:
        return [[v, n] for v in [0, -1, 1, BIG, 'a', 1.0] for n in [-65, -1, 0, 1, 65, 'a', 1.0]]
    return [[v, w] for v, w in [(0, 0), (-1, 1), (BIG, BIG), (-BIG, 3),
                              (1, 0), (1.0, 2), (2, 1.0), ('a', 1), (1, 'a')]]


def helper(text, name):
    """Retain all clauses of a selected helper, ending at its actual function terminator."""
    found = re.findall(rf'^{name}\(.*?\.\s*$', text, re.M | re.S)
    assert len(found) == 1, name
    return found[0].strip()


def cases(work):
    """Keep legality, alias shadowing and every unavailable identity visible even in skipped branches."""
    rows = []
    for (name, arity), owner in BLOCKED.items():
        args = ','.join('X' for _ in range(arity))
        for qualified in [False, True]:
            for skipped in [False, True]:
                label = f'gate_{name}_{arity}_{int(qualified)}_{int(skipped)}'
                call = ('erlang:' if qualified else '') + name + '(' + args + ')'
                body = f'f(X) when ' + ('true orelse ' if skipped else '') + call + ' -> X.'
                rows.append(dict(name=label, body=body, diagnostic='', capability='guards', owner=owner))
    extra = [
        ('legacy_record_shadow', '-record(r,{a}). -compile({no_auto_import,[is_record/2]}). f(X) when record(X,r) -> X. is_record(X,Y) -> {X,Y}.', 'illegal guard call'),
        ('legacy_record_old', '-record(r,{a}). f(X) when record(X,r) -> X. record(X,Y) -> {X,Y}.', ''),
        ('legacy_record_suppress', '-record(r,{a}). -compile({no_auto_import,[record/2]}). f(X) when record(X,r) -> X.', ''),
        ('legacy_integer_suppress', '-compile({no_auto_import,[is_integer/1]}). f(X) when integer(X) -> X.', ''),
        ('quoted_operator', "f(X) when '+'(X,1) -> X.", 'illegal guard call'),
        ('wrong_range_arity', 'f(X) when is_integer(X,X) -> X.', 'illegal guard call'),
    ]
    for name, body, error in extra:
        rows.append(dict(name=name, body=body, diagnostic=error, capability=''))
    for row in rows:
        (work/(row['name']+'.erl')).write_bytes(f'-module({row["name"]}).\n% Guard catalog semantic case.\n{row["body"]}\n'.encode())
    (work/'patterns.term').write_bytes(('\n'.join(f'{{{r["name"]},{"rejected" if r["diagnostic"] else "accepted"},none}}.' for r in rows)+'\n').encode())
    return rows


def kernels(otp, work):
    """Map each catalog row to native helpers; retain selected complete OTP functions and labeled adaptations."""
    source = pathlib.Path(__file__).resolve().parents[3]
    rows = list(csv.DictReader((source/'tests/fixtures/patternmatch/guards.tsv').read_text(encoding='utf8').splitlines(), delimiter='\t'))
    text = (otp/'lib/compiler/test/guard_SUITE.erl').read_text(encoding='utf8')
    definitions, calls, audit = [], [], []
    for index, row in enumerate(rows):
        name, arity = row['name'].strip("'"), int(row['arity'])
        entry = dict(row, fixture='generated/guard_catalog', resolver='semantic/pattern_calls.cpp',
                     lowering='codegen/lowering_immediates.cpp',
                     runtime_owner='runtime/src/terms/{immediate_services,container_services,structural_order}.cpp')
        if (name, arity) in BLOCKED:
            audit.append(dict(entry, status='dependency-blocked', dependency=BLOCKED[name, arity]))
            continue
        legacy = row['category'] == 'old_type_test'
        modern = 'is_'+name if legacy else name
        if modern == 'is_record' or (modern == 'is_integer' and arity == 3):
            entry.update(lowering='codegen/lowering_record_tests.cpp', runtime_owner='composition of checked integer/tuple/predicate/comparison services')
        elif modern in ['map_size','map_get','is_map_key']:
            entry.update(lowering='codegen/lowering_maps.cpp', runtime_owner='runtime/src/terms/map_services.cpp')
        elif modern in ['bit_size','byte_size','binary_part']:
            entry.update(lowering='codegen/lowering_bits.cpp', runtime_owner='runtime/src/terms/bit_services.cpp')
        elif row['category'] == 'arith_op' or modern in ['abs','float','round','trunc','floor','ceil']:
            entry.update(lowering='codegen/lowering_{integers,floats}.cpp', runtime_owner='runtime/src/terms/{integer_service,numeric_service}.cpp')
        params = ['X'] if modern == 'is_record' else ['X', 'Y', 'Z'][:arity]
        query = ','.join(params)
        operands = query + (',r' if arity == 2 else ',r,2') if modern == 'is_record' else query
        ordinary = f"erlang:'{modern}'({operands})"
        guard = f'{name}({operands})' if legacy else ordinary
        names = []
        for kind in ['body', 'qualified', 'guard']:
            fn = f'catalog_{index}_{kind}'
            names.append(fn)
            body = f'{fn}({query}) -> {ordinary}.' if kind != 'guard' else f'{fn}({query}) when {guard} -> ok; {fn}({query}) -> no.'
            if kind == 'body' and row['category'] in ['guard_bif', 'new_type_test']:
                body = body.replace("erlang:'"+modern+"'", modern)
            definitions.append((fn, len(params), body))
            calls += [('answer', fn, args) for args in arguments(modern, arity)]
        audit.append(dict(entry, status='implemented', functions=names,
                          calls_per_function=len(arguments(modern, arity)),
                          domain='admitted values only; unavailable function/pid/port/reference classes always false'))
    selected = []
    for name, arity in [(f'is_integer_3_guard_{n}', 1 if n in [2,3] else 3) for n in range(1,9)] + [(f'is_integer_3_guard_{n}_id',1) for n in [4,5,8]]:
        body = helper(text, name)
        definitions.append((name, arity, body))
        selected.append(dict(source='lib/compiler/test/guard_SUITE.erl', function=f'{name}/{arity}', clause=body))
        if not name.endswith('_id'):
            calls += [('answer', name, args) for args in arguments('is_integer',3)] if arity == 3 else [('answer',name,[v]) for v in VALUES+[1024,1025]]
    bif = (otp/'lib/compiler/test/bif_SUITE.erl').read_text(encoding='utf8')
    for name, arity in [('bool_min_false',2),('bool_min_true',2),('bool_max_false',2),('max_number',1),('min_increment',1),('int_clamped_add',1),('num_clamped_add',1)]:
        body = helper(bif,name)
        definitions.append((name,arity,body))
        selected.append(dict(source='lib/compiler/test/bif_SUITE.erl',function=f'{name}/{arity}',clause=body))
        calls += [('answer',name,a) for a in arguments('min',2)] if arity == 2 else [('answer',name,[v]) for v in VALUES]
    maps = (otp/'lib/compiler/test/map_SUITE.erl').read_text(encoding='utf8')
    for name, arity in [('map_guard_empty',0),('map_guard_empty_2',0),('map_get_head',1),('map_get_head_not',1),('map_is_key_head',1),('map_is_key_head_not',1),('map_get_head_badmap1',0),('map_get_head_badmap2',0),('map_get_head_badmap3',0),('map_field_check_sequence',1)]:
        body = helper(maps,name)
        definitions.append((name,arity,body))
        selected.append(dict(source='lib/compiler/test/map_SUITE.erl',function=f'{name}/{arity}',clause=body))
        calls += [('answer',name,[])] if arity == 0 else [('answer',name,[v]) for v in VALUES+[{'map':[]},{'map':[('a','false')]}]]
    definitions += [('id',1,'id(X) -> X.')]
    adaptations = []
    for name in ['trunc','round','floor','ceil']:
        fn='conversion_'+name
        body=f'{fn}(X) when {name}(X) == float({name}(X)) -> {name}(X); {fn}(_) -> no.'
        definitions.append((fn,1,body))
        calls += [('answer',fn,[v]) for v in VALUES+[-1.5,0.5,42.77]]
        adaptations.append(dict(source='lib/compiler/test/bif_SUITE.erl',function='trunc_and_friends/1 -> trunc_template/2', adaptation='retain numeric exact/nonexact conversion relations; replace if/try/meta-generation with ordered guards and body; wrong-type body calls covered by catalog rows',clause=body))
    for name, arity, body in [
        ('construct',1,'construct(X) when element(2,{X,[X,#{a => <<1:3>>}]}) =:= [X,#{a => <<1:3>>}] -> ok; construct(_) -> no.'),
        ('update',1,'update(M) when map_get(a,M#{a := {1,[2],<<3:4>>}}) =:= {1,[2],<<3:4>>}; is_map(M) -> ok; update(_) -> no.'),
        ('range_alt',3,'range_alt(X,L,U) when is_integer(X,L,U); X =:= a -> yes; range_alt(_,_,_) -> no.'),
    ]:
        definitions.append((name,arity,body))
        calls += [('answer',name,args) for args in (arguments('is_integer',3) if arity == 3 else [[v] for v in VALUES+[{'map':[]}]] )]
    exports=','.join(f'{n}/{a}' for n,a,_ in definitions)
    (work/'answer.erl').write_bytes((text.split('-module(')[0]+f'-module(answer).\n-export([{exports}]).\n-record(r,{{a}}).\n'+'\n'.join(b for _,_,b in definitions)+'\n').encode())
    (work/'client.erl').write_bytes(b'-module(client).\n-export([range/3,retry/1]).\nrange(X,L,U) -> answer:range_alt(X,L,U).\nretry(X) -> answer:construct(X).\n')
    calls += [('client','range',[0,-BIG,BIG]),('client','range',['a','bad',0]),('client','retry',[MAP])]
    write_calls(work,calls)
    return dict(calls=len(calls),catalog=audit,cases=cases(work),helpers=selected,adaptations=adaptations,
                source_sha256={f'lib/compiler/test/{n}_SUITE.erl':digest(otp/f'lib/compiler/test/{n}_SUITE.erl') for n in ['guard','bif','map']},
                catalog_sha256=digest(source/'tests/fixtures/patternmatch/guards.tsv'))


def main():
    tool,cmake,root,directory,settings,config,suffix=sys.argv[1:]
    source,work=pathlib.Path(root),pathlib.Path(directory)
    evidence=load(source,'guard_catalog',work)
    assert evidence['catalog_sha256'] == digest(source/'tests/fixtures/patternmatch/guards.tsv')
    (work/'out').mkdir(exist_ok=True)
    (work/'out/sentinel').write_bytes(b'preserve')
    for row in evidence['cases']:
        if row['diagnostic'] or row['capability']:
            for policy in [['-O0'],['-O2'],['-O0','--no-type-specialization'],['-O2','--no-type-specialization']]:
                for project in [False,True]:
                    compile_case(tool,work,row,policy,project)
        else:
            run([tool,'--print-types',str(work/(row['name']+'.erl'))])
    assert list((work/'out').iterdir()) == [work/'out/sentinel']
    (work/'project.toml').write_bytes((source/'tests/fixtures/patternmatch/generated/guard_catalog/project.toml').read_bytes())
    native(tool,cmake,source,work,settings,config,suffix)
    (work/'evidence.json').write_bytes((json.dumps(evidence,indent=2)+'\n').encode())
    print(f'{len(evidence["catalog"])} catalog signatures; {evidence["calls"]} native outcomes in four policies; {len(evidence["cases"])} legality/gate cases.')


if __name__ == '__main__':
    main()
