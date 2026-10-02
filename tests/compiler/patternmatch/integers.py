"""Compare arbitrary integer construction, promotion, guards and matching with OTP."""
import itertools
import json
import pathlib
import re
import random
import subprocess
import sys
from evidence import digest, provenance, run
from immediate import native
from services import write_calls


def kernels(otp, work):
    """Retain complete arithmetic helpers and label bounds-suite operator adaptations explicitly."""
    paths = [otp / f'lib/compiler/test/{name}.erl' for name in ['trycatch_SUITE', 'beam_bounds_SUITE']]
    sources = [path.read_text(encoding='utf8') for path in paths]
    definitions, helpers = [], []
    for source, name, arity in [(sources[0], 'my_div', 2), (sources[0], 'my_add', 2),
                                (sources[1], 'bnot_bounds_2_coverage', 1)]:
        body = re.search(r'^' + name + r'\(.*?\.(?=\n)', source, re.M | re.S).group()
        definitions.append((name, arity, body))
        helpers.append({'function': f'{name}/{arity}', 'body': body, 'adaptations': 'none'})
    operations = [('add','+'),('subtract','-'),('multiply','*'),('divide','div'),('remainder','rem'),
                  ('bit_and','band'),('bit_or','bor'),('bit_xor','bxor'),('left','bsl'),('right','bsr')]
    for name, op in operations:
        definitions += [(name, 2, f'{name}(X,Y) -> X {op} Y.'),
                        ('guard_'+name, 2, f'guard_{name}(X,Y) when (X {op} Y) =:= 0 -> zero; guard_{name}(X,Y) when is_integer(X {op} Y) -> integer; guard_{name}(_,_) -> rejected.')]
    definitions += [(name, 1, f'{name}(X) -> {op} X.') for name,op in [('positive','+'),('negative','-'),('complement','bnot')]]
    definitions += [
        ('absolute',1,'absolute(X) -> abs(X).'),
        ('classify',1,'classify(X) -> {is_integer(X),is_number(X),is_float(X)}.'),
        ('arity',1,'arity(X) -> is_function(ok,X).'),
        ('element_index',1,'element_index(X) -> element(X,{a,b}).'),
        ('legacy',1,'legacy(X) when integer(X) -> X; legacy(_) -> no.'),
        ('qualified',2,"qualified(X,Y) -> erlang:'+'(X,Y)."),
        ('roundtrip',1,'roundtrip(X) -> A = (X bsl 130) + 7, (A - 7) bsr 130.'),
        ('literal',1,'literal(1361129467683753853853498429727072845824) -> yes; literal(_) -> no.'),
        ('negative_literal',1,'negative_literal(-1361129467683753853853498429727072845824) -> yes; negative_literal(_) -> no.'),
        ('constant',0,'constant() -> {1361129467683753853853498429727072845824,-1361129467683753853853498429727072845824}.'),
        ('body',1,'body(X) -> 1361129467683753853853498429727072845824 = X, X.'),
        ('repeat',2,'repeat(X,X) -> same; repeat(_,_) -> different.'),
        ('independent',1,'independent(X) -> A = {X bsl 130,[X bsl 131]}, B = {X bsl 130,[X bsl 131]}, A = B, A =:= B.'),
        ('compare',2,'compare(X,Y) -> {X =:= Y,X == Y,X < Y,X =< Y,X > Y,X >= Y,min(X,Y),max(X,Y)}.'),
        ('wrong_spec',2,'-spec wrong_spec(integer(),integer()) -> integer().\nwrong_spec(X,Y) -> X + Y.'),
        ('error_order',2,'error_order(X,Y) -> {X div Y, ok = impossible}.'),
        ('id',1,'id(I) -> I.')]
    exports = ','.join(f'{name}/{arity}' for name,arity,_ in definitions)
    notices = '\n'.join(source.split('-module(')[0] for source in sources)
    (work/'answer.erl').write_text(notices + f'-module(answer).\n-export([{exports}]).\n' +
                                  '\n'.join(body for _,_,body in definitions)+'\n',encoding='utf8')
    (work/'client.erl').write_text('-module(client).\n-export([nested/2,retain/1]).\n'
        'nested(X,Y) -> answer:my_add(answer:my_div(X,Y),X).\n'
        'retain(X) -> A = answer:roundtrip(X), answer:independent(X), answer:id(A).\n',encoding='utf8')
    values = [-2**140,-2**130,-2**63,-2**59-1,-2**59,-2**27-1,-3,-1,0,1,2,3,
              2**27-1,2**27,2**59-1,2**59,2**63,2**130,2**140,'a',[],(),(1,)]
    calls = [('answer',name,[value]) for name,arity,_ in definitions if arity==1 for value in values]
    ordinary = [name for name,arity,_ in definitions if arity==2 and name not in ['left','right','guard_left','guard_right']]
    calls += [('answer',name,list(pair)) for name in ordinary for pair in itertools.product(values,repeat=2)]
    calls += [('answer',name,[value,count]) for name in ['left','right','guard_left','guard_right']
              for value in values for count in [-140,-60,-28,-1,0,1,27,28,59,60,100,'a']]
    calls += [('answer','right',[value,2**130]) for value in [-2**140,-3,0,1,2**140]]
    calls += [('answer','left',[0,2**130]),('answer','constant',[])]
    calls += [('client','nested',list(pair)) for pair in itertools.product(values,[-3,0,1,2,'a'])]
    calls += [('client','retain',[value]) for value in values]
    random_values = random.Random(29013)
    for _ in range(256):
        pair = [random_values.getrandbits(random_values.randrange(1,401)) * random_values.choice([-1,1]) for _ in range(2)]
        calls += [('answer', name, pair) for name in ['add','subtract','multiply','divide','remainder','bit_and','bit_or','bit_xor','compare']]
    write_calls(work,calls)
    return {'calls':len(calls),'sources':{str(path.relative_to(otp)):digest(path) for path in paths},
            'helpers':helpers,'declarations':exports,'wrapper_sha256':digest(work/'answer.erl'),
            'client_sha256':digest(work/'client.erl'),
            'adaptations':'beam_bounds arithmetic/bitwise/shift kernels use direct complete wrappers instead of higher-order private bounds harnesses; authored boundary, promotion, independent allocation, comparison, failure and guard kernels preserve their tested operations'}


def main():
    tool,cmake,root,otp_root,directory,settings,config,suffix,escript = sys.argv[1:]
    source,otp,work = pathlib.Path(root),pathlib.Path(otp_root),pathlib.Path(directory)
    work.mkdir(parents=True,exist_ok=True)
    provenance(source,otp,source/'tests/fixtures/patternmatch',work)
    records = kernels(otp,work)
    expected = run([escript,str(source/'tests/compiler/patternmatch/immediate.escript'),str(work)])
    (work/'expected.txt').write_bytes(expected.replace('\r\n','\n').encode())
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert all(name in ir for name in ['add i128','sub i128','mul i128','integer.fallback','integer.outcome'])
    for bits,triple in [(32,'i686-pc-windows-msvc'),(64,'x86_64-pc-windows-msvc')]:
        run([tool,'--target-triple',triple,'--emit','obj','--artifact-dir',str(work/f'width{bits}'),str(work/'answer.erl'),str(work/'client.erl')])
    for body in ['f() -> NUMBER.', 'f(NUMBER) -> ok.']:
        path = work/'excessive.erl'
        path.write_text('-module(excessive).\n'+body.replace('NUMBER','9'*10001)+'\n',encoding='utf8')
        result = subprocess.run([tool,'--emit','obj','--artifact-dir',str(work/'invalid'),str(path)],
                                capture_output=True,text=True,encoding='utf8',timeout=30)
        assert result.returncode == 1 and 'integer literal digit limit exceeded' in result.stderr, result.stderr
        assert not (work/'invalid').exists()
    (work/'evidence.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
    print(f'{records["calls"]} exact integer OTP/native calls passed in four policies; both target payload widths emitted.')


if __name__ == '__main__':
    main()
