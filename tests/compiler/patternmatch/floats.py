"""Compare finite binary64 arithmetic, exact matching and mixed numeric order with OTP."""
import itertools
import json
import math
import pathlib
import random
import re
import sys
from stored import load
from evidence import digest, provenance, run
from immediate import native
from services import write_calls


def kernels(otp, work):
    """Preserve pc/3 verbatim and adapt case-based comparison into ordered guard clauses."""
    paths = [otp / f'lib/compiler/test/{name}.erl' for name in ['float_SUITE', 'beam_type_SUITE']]
    sources = [path.read_text(encoding='utf8') for path in paths]
    pc = re.search(r'^pc\(.*?\.(?=\n)', sources[0], re.M | re.S).group()
    definitions = [('pc', 3, pc)]
    for name, operator in [('add','+'),('subtract','-'),('multiply','*'),('divide','/')]:
        definitions += [(name, 2, f'{name}(X,Y) -> X {operator} Y.'),
            ('guard_'+name, 2, f'guard_{name}(X,Y) when is_number(X {operator} Y) -> yes; guard_{name}(_,_) -> no.')]
    for name in ['float','round','trunc','floor','ceil','abs']:
        definitions += [('value_'+name, 1, f'value_{name}(X) -> erlang:{name}(X).'),
            ('guard_'+name, 1, f'guard_{name}(X) when is_number(erlang:{name}(X)) -> yes; guard_{name}(_) -> no.')]
    definitions += [
        ('compare',2,'compare(X,Y) -> {X =:= Y,X =/= Y,X == Y,X /= Y,X < Y,X =< Y,X > Y,X >= Y,min(X,Y),max(X,Y)}.'),
        ('nested',2,'nested(X,Y) -> {{X,[X]} =:= {Y,[Y]},{X,[X]} == {Y,[Y]},{X,[X]} < {Y,[Y]}}.'),
        ('repeat',2,'repeat(X,X) -> same; repeat(_,_) -> different.'),
        ('literal',1,'literal(1.0) -> one; literal(-0.0) -> negative_zero; literal(0.0) -> positive_zero; literal(_) -> other.'),
        ('body',1,'body(X) -> 1.0 = X, X.'),
        ('constant',0,'constant() -> {1.0,-0.0,0.0,1.7976931348623157e308,4.9406564584124654e-324}.'),
        ('positive',1,'positive(X) -> +X.'),
        ('negative',1,'negative(X) -> -X.'),
        ('classify',1,'classify(X) -> {is_number(X),is_float(X),is_integer(X)}.'),
        ('integer_only',1,'integer_only(X) -> X div 1.'),
        ('legacy',1,'legacy(X) when float(X) -> yes; legacy(_) -> no.'),
        ('float_compare',1,'float_compare(X) when X > 0 -> X + 1.0 > 0; float_compare(X) -> X + 1.0, false.'),
        ('wrong_spec',2,'-spec wrong_spec(integer(),integer()) -> integer().\nwrong_spec(X,Y) -> X + Y.'),
        ('ordered',2,'ordered(X,Y) -> {X / Y, ok = impossible}.'),
        ('id',1,'id(X) -> X.')]
    exports = ','.join(f'{name}/{arity}' for name,arity,_ in definitions)
    notices = '\n'.join(source.split('-module(')[0] for source in sources)
    (work/'answer.erl').write_text(notices + f'-module(answer).\n-export([{exports}]).\n' +
        '\n'.join(body for _,_,body in definitions)+'\n',encoding='utf8')
    (work/'client.erl').write_text('-module(client).\n-export([nested/2,retain/1]).\n'
        'nested(X,Y) -> answer:add(answer:divide(X,Y),X).\n'
        'retain(X) -> A = answer:id(X), answer:constant(), answer:id(A).\n',encoding='utf8')
    values = [-2**1100,-2**1000,-2**53-1,-2**53,-42,-1,0,1,2**53,2**53+1,2**1000,2**1100,
        -1.7976931348623157e308,-float(2**53),-42.0,-2.5,-1.5,-0.5,-0.0,0.0,5e-324,
        0.5,1.0,1.5,2.5,42.0,float(2**53),1.7976931348623157e308,'a',[],(),(1,)]
    calls = [('answer',name,[v]) for name,arity,_ in definitions if arity == 1 for v in values]
    calls += [('answer',name,list(pair)) for name,arity,_ in definitions if arity == 2
              for pair in itertools.product(values,repeat=2)]
    calls += [('answer','constant',[]),('answer','pc',[77,23,5]),('answer','pc',[1,0,-0.0]),('answer','pc',[0,0,1])]
    calls += [('client','nested',[v,0.0]) for v in values]
    calls += [('client','retain',[v]) for v in values]
    seeded = random.Random(29014)
    for _ in range(64):
        real = math.ldexp(seeded.uniform(-1,1),seeded.randrange(-1070,1024))
        for v in [int(real),int(real)-1,int(real)+1]:
            calls += [('answer','compare',[real,v]),('answer','value_float',[v])]
    write_calls(work,calls)
    return {'calls':len(calls),'sources':{str(p.relative_to(otp)):digest(p) for p in paths},
        'helpers':[{'function':'float_SUITE:pc/3','body':pc,'adaptations':'none'},
        {'function':'beam_type_SUITE:float_compare/1','adaptations':'do_float_compare case becomes ordered clauses; all six original numeric inputs retained; positive branch evaluates Y > 0, nonpositive evaluates addition then false'}],
        'wrapper_sha256':digest(work/'answer.erl'),'client_sha256':digest(work/'client.erl')}


def main():
    tool,cmake,root,directory,settings,config,suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True,exist_ok=True)
    records = load(source, "floats", work)
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert 'float.outcome' in ir
    assert not re.search(r'\b(fast|nnan|ninf|nsz|arcp|reassoc|afn)\b',ir)
    for bits,triple in [(32,'i686-pc-windows-msvc'),(64,'x86_64-pc-windows-msvc')]:
        run([tool,'--target-triple',triple,'--emit','obj','--artifact-dir',str(work/f'width{bits}'),str(work/'answer.erl'),str(work/'client.erl')])
    (work/'evidence.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
    print(f'{records["calls"]} float OTP/native calls passed in four policies; both word widths emitted.')


if __name__ == '__main__':
    main()
