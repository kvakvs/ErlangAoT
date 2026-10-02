"""Finish the owned corpus with reproducible nested stress and a provenance/coverage reconciliation."""
import json
import pathlib
import random
import sys
from evidence import digest,run
from immediate import native
from services import write_calls
from stored import load

SEED=0x29A07


def value(rng,depth):
    """Bound every generated term while varying exact keys, improper tails, floats and shared shape."""
    scalars=[0,-1,1,1<<100,-(1<<100),0.0,-0.0,1.5,'a','true','false','Ω']
    if depth==0: return rng.choice(scalars)
    kind=rng.randrange(6)
    if kind==0: return tuple(value(rng,depth-1) for _ in range(rng.randrange(4)))
    if kind==1: return [value(rng,depth-1) for _ in range(rng.randrange(4))]
    if kind==2: return {'cons':[value(rng,depth-1),value(rng,0)]}
    if kind==3: return {'map':[(k,value(rng,depth-1)) for k in rng.sample(['a',1,1.0,()],rng.randrange(4))]}
    if kind==4:
        bits=rng.randrange(65); count=(bits+7)//8; data=bytearray(rng.randbytes(count))
        if bits%8: data[-1]&=255<<(8-bits%8)
        return {'bits':data.hex(),'length':bits}
    return value(rng,0)


def kernels(otp,work):
    """Retain the upstream license and explicitly label seeded extensions of success-register matching."""
    suite=otp/'lib/compiler/test/match_SUITE.erl'; text=suite.read_text(encoding='utf8')
    assert 'force_succ_regs(_A, B) -> B.' in text
    rng=random.Random(SEED); values=[value(rng,3) for _ in range(160)]
    deep='a'
    for depth in range(1,65):
        deep=(deep,) if depth%2 else [deep]
        if depth in [16,32,64]: values.append(deep)
    values += [tuple(range(255)),list(range(255)),{'map':[(i,i) for i in range(255)]},
               {'bits':'ab'*256,'length':2048},('r',1,2),('r',1),('s',1,2),{'map':[('k',1)]}]
    definitions=[
        ('identity',1,'identity(X) -> X.'),
        ('repeat',2,'repeat(X,X) -> {same,X}; repeat(X,Y) -> {different,X,Y}.'),
        ('compare',2,'compare(X,Y) -> {X =:= Y,X == Y,X < Y,X > Y}.'),
        ('staged',1,'staged(X) -> Y={X,[X],#{k => X},<<17:5>>}, {A,[B],#{k := C},<<D:5>>}=Y, {A,B,C,D}.'),
        ('select',1,'select(#r{a=X,b=X}) -> {record,X}; select({X,X}) -> {tuple,X}; select([X,X]) -> {list,X}; select(#{k := X}) -> {map,X}; select(<<X:8,T/bitstring>>) -> {bits,X,T}; select(X) -> {other,X}.'),
        ('guard',1,'guard(X) when element(1,X)==1,tuple_size(X)==2; map_get(k,X)==1; is_integer(X,-100,100) -> pass; guard(_) -> reject.'),
        ('wide',0,'wide() -> {'+','.join(map(str,range(255)))+'}.'),
        ('alternatives',1,'; '.join(f'alternatives({{{n},X}}) when is_integer(X,{n},{n}) -> X' for n in range(128))+'; alternatives(X) -> X.'),
    ]
    exports=','.join(f'{n}/{a}' for n,a,_ in definitions)
    (work/'answer.erl').write_bytes((text.split('-module(')[0]+f'-module(answer).\n-export([{exports}]).\n-record(r,{{a,b}}).\n-spec staged(integer()) -> integer().\n'+'\n'.join(b for _,_,b in definitions)+'\n').encode())
    (work/'client.erl').write_bytes(b'-module(client).\n-export([run/1,retry/1]).\nrun(X) -> answer:staged(answer:identity(X)).\nretry(X) -> answer:guard(X).\n')
    calls=[]
    for v in values:
        for fn in ['identity','staged','select','guard']: calls.append(('answer',fn,[v]))
        for fn in ['repeat','compare']: calls += [('answer',fn,[v,v]),('answer',fn,[v,rng.choice(values)])]
        calls += [('client','run',[v]),('client','retry',[v])]
    calls += [('answer','alternatives',[(n,m)]) for n in range(128) for m in [n,n+1]]
    calls += [('answer','wide',[]),('client','retry',[1]),('client','retry',['a'])]
    write_calls(work,calls)
    return dict(calls=len(calls),seed=SEED,values=len(values),max_generated_depth=64,wide=255,alternatives=128,
                source=str(suite.relative_to(otp)),source_sha256=digest(suite),
                adaptations='authored bounded seeded extensions of repeated binding, successful match registers, representation ordering, nested rooted construction and guard rejection; no Common Test harness execution')


def reconcile(source):
    """Verify all owned bytes and require a concrete executable or dependency row for every signature."""
    root=source/'tests/fixtures/patternmatch/generated'; rows=[]
    for path in sorted(root.glob('*/manifest.json')):
        manifest=json.loads(path.read_text(encoding='utf8')); assert manifest['schema']==1
        for name,hashvalue in manifest['files'].items():
            assert pathlib.Path(name).name==name and digest(path.parent/name)==hashvalue,(path,name)
        rows.append(dict(corpus=manifest['corpus'],manifest_sha256=digest(path),files=len(manifest['files']),
                         expected_rows=len((path.parent/'expected.txt').read_text(encoding='utf8').splitlines()),
                         revision=manifest['provenance']['revision'] if 'revision' in manifest['provenance'] else manifest['provenance']))
    audit=json.loads((root/'guard_catalog/manifest.json').read_text(encoding='utf8'))['evidence']['catalog']
    assert len(audit)==81 and sum(r['status']=='dependency-blocked' for r in audit)==4
    answer=(root/'guard_catalog/answer.erl').read_text(encoding='utf8')
    for row in audit:
        if row['status']=='implemented':
            assert row['resolver'] and row['lowering'] and row['runtime_owner'] and row['calls_per_function']>0
            for function in row['functions']: assert function+'(' in answer
        else: assert row['dependency']
    return rows


def main():
    tool,cmake,root,directory,settings,config,suffix=sys.argv[1:]
    source,work=pathlib.Path(root),pathlib.Path(directory)
    evidence=load(source,'closure',work); evidence['corpora']=reconcile(source)
    native(tool,cmake,source,work,settings,config,suffix)
    (work/'evidence.json').write_bytes((json.dumps(evidence,indent=2)+'\n').encode())
    print(f'{evidence["calls"]} seeded/deep/wide outcomes in four policies; {len(evidence["corpora"])} owned corpus manifests reconciled.')

if __name__=='__main__': main()
