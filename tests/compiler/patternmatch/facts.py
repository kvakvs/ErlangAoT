"""Compare paired misleading contracts and conservative binding facts through real native workflows."""
import itertools
import json
import pathlib
import re
import sys
from evidence import digest,run
from immediate import native
from services import write_calls
from stored import load
from guard_catalog import BIG,MAP,BIN,BITS,VALUES


def kernels(otp,work):
    suite=otp/'lib/compiler/test/match_SUITE.erl'
    text=suite.read_text(encoding='utf8')
    witness='force_succ_regs('
    assert witness in text
    definitions=[
        ('identity',1,'identity(X) -> Y=X, id(Y).'),
        ('constant',0,'constant() -> Y=42, Z=Y, id(Z).'),
        ('alias',1,'alias(X) -> (A=B)=X, {A,B}.'),
        ('projected',2,'projected(X,Y) -> A=X, B=Y, id(A), B.'),
        ('extracted',1,'extracted({X,{Y,Z}}) when is_integer(X),is_integer(Y,0,10) -> {X,Y,Z}; extracted([X|Y]) -> {X,Y}; extracted(X) -> {fallback,X}.'),
        ('joined',1,'joined({_,Z}) -> Z; joined([X|_]) -> X; joined(X) -> X.'),
        ('map',2,'map(K,M) -> #{K := V}=M, [V,map_get(K,M)].'),
        ('bits',1,'bits(<<N:8,V:N,T/bitstring>>) -> {N,V,T}; bits(_) -> no.'),
        ('record',1,'record(#r{a=X,b=Y}) when is_tuple(X) -> {Z}=X, Y=Z, Y; record(_) -> no.'),
        ('allocation',1,'allocation(X) -> A={X,[X],#{key=>X},<<3:2>>}, {_,[Y],#{key:=Z},<<V:2>>}=A, {Y,Z,V}.'),
        ('candidate',1,'candidate({X,Y}) when is_tuple(X),element(1,X)==Y -> X; candidate({X,Y}) when is_list(X),hd(X)==Y -> Y; candidate(V) -> V.'),
        ('guards',1,f'guards(X) when element(1,X)==true; map_get(ok,X)==true; is_integer(X,{-BIG},{BIG}) -> Y=X,Y; guards(X) -> {{fallback,X}}.'),
    ]
    selected=re.findall(r'^force_succ_regs\(.*?\.\s*$',text,re.M|re.S)
    assert len(selected)==1
    definitions.append(('force_succ_regs',2,selected[0].strip()))
    values=VALUES+[('r',(1,),1),('r',(1,),2), (1,(0,BITS)),(1,(11,MAP)),('true',),{'map':[('ok','true')]},
                   ([1],1), ((),1),((1,),1), (('false',),1),{'bits':'0380','length':9}]
    calls=[]
    wrapped=[]
    for name,arity,body in definitions:
        for annotated in [False,True]:
            fn=name+('_spec' if annotated else '_plain')
            copied=re.sub(r'\b'+name+r'\(',fn+'(',body)
            if annotated:
                copied=f'-spec {fn}('+','.join(['integer()']*arity)+') -> integer().\n'+copied
            wrapped.append((fn,arity,copied))
            args=[[]] if arity==0 else [[v] for v in values] if arity==1 else [[v,w] for v,w in itertools.product(values[:8],repeat=2)]
            if name=='map': args=[[k,m] for k in ['a',1,1.0,'missing'] for m in [MAP,{'map':[]},'a']]
            calls += [('answer',fn,a) for a in args]
    exports=','.join(f'{n}/{a}' for n,a,_ in wrapped)+',id/1'
    (work/'answer.erl').write_bytes((text.split('-module(')[0]+f'-module(answer).\n-export([{exports}]).\n-record(r,{{a,b}}).\n'+'\n'.join(b for _,_,b in wrapped)+'\nid(X) -> X.\n').encode())
    (work/'client.erl').write_bytes(b'-module(client).\n-export([run/1,retry/1]).\n-spec run(integer()) -> integer().\nrun(X) -> answer:allocation_spec(answer:identity_spec(X)).\nretry(X) -> answer:joined_plain(X).\n')
    calls += [('client','run',[v]) for v in values]+[('client','retry',[v]) for v in values]
    write_calls(work,calls)
    return dict(calls=len(calls),source=str(suite.relative_to(otp)),source_sha256=digest(suite),
                helpers=[dict(function='force_succ_regs/2',clause=selected[0].strip())],
                adaptations='rename complete force_succ_regs helper twice; paired authored body-binding, projection, checked extraction/allocation and failed-candidate kernels extend its success-register obligation; misleading integer specs affect no implementation facts',
                pairs=[n for n,_,_ in definitions],values=len(values))


def dominance(ir):
    """Use LLVM dominator analysis for checked services, independently of source specs or block order."""
    # Each generated service publishes output only on its success edge. Check the corresponding
    # call/result-use blocks through the actual IR CFG, including cleanup and rejection paths.
    results=[]
    for body in re.findall(r'^define .*?^}',ir,re.M|re.S):
        lines=body.splitlines(); blocks={}; current=None
        for line in lines[1:]:
            label=re.match(r'^([\w.]+):',line)
            if label:
                current=label[1]; blocks[current]=[]
            elif current: blocks[current].append(line)
        if not blocks: continue
        pred={name:set() for name in blocks}
        for name,content in blocks.items():
            for target in re.findall(r'label %([\w.]+)','\n'.join(content)):
                pred[target].add(name)
        first=next(iter(blocks)); dom={b:({b} if b==first else set(blocks)) for b in blocks}
        changed=True
        while changed:
            changed=False
            for b in blocks:
                if b==first: continue
                parents=pred[b]
                value={b}|(set.intersection(*(dom[p] for p in parents)) if parents else set())
                if value!=dom[b]: dom[b]=value; changed=True
        outcomes={}
        inspections=[]
        conditions={}
        branches={}
        for b,content in blocks.items():
            for line in content:
                inspect=re.search(r'%([\w.]+) = call i8 .*erlang_aot_inspect_v1.*i8 ([0-4]), i64 %([\w.]+), i64 \d+, ptr %([\w.]+)\)',line)
                if inspect: inspections.append((inspect[1],int(inspect[2]),inspect[3],b))
                call=re.search(r'%([\w.]+) = call i8 .*ptr %([\w.]+)\)',line)
                if call and '.outcome' in call[1]: outcomes[call[2]]=(call[1],b)
                check=re.search(r'%([\w.]+) = icmp eq i8 %([\w.]+), 0',line)
                if check: conditions[check[2]]=(check[1],b)
                edge=re.search(r'br i1 %([\w.]+), label %([\w.]+), label %([\w.]+)',line)
                if edge: branches[edge[1]]=(b,edge[2],edge[3])
        assert 'inttoptr' not in body, 'Generated source contains an unchecked heap pointer conversion'
        for b,content in blocks.items():
            for line in content:
                loaded=re.search(r'%service.value[\w.]* = load i64, ptr %([\w.]+)',line)
                if not loaded: continue
                assert loaded[1] in outcomes,(b,line)
                outcome,call_block=outcomes[loaded[1]]
                condition,check_block=conditions[outcome]
                edge_owner,success,rejection=branches[condition]
                assert call_block in dom[b] and check_block in dom[b] and success in dom[b],(outcome,b,line)
                assert edge_owner == check_block and rejection != success
                results.append(dict(function=lines[0].split('@')[1].split('(')[0],load_block=b,
                                    call_block=call_block,check_block=check_block,success_block=success))
        for outcome,operation,value,block in inspections:
            if operation not in [1,3,4]: continue
            shape=0 if operation==1 else 2
            proved=[]
            for test,kind,candidate,owner in inspections:
                if kind!=shape or candidate!=value: continue
                condition,check_block=conditions[test]
                success=branches[condition][1]
                if success in dom[block]: proved.append(success)
            assert proved,(operation,value,block)
            results.append(dict(kind='shape-before-extraction',function=lines[0].split('@')[1].split('(')[0],
                                extract_block=block,shape_success_blocks=proved))
        for block,content in blocks.items():
            if any('binary.cursor' in line and 'load i64' in line for line in content):
                assert any('service.value' in line and 'load i64' in line for line in content),block
                results.append(dict(kind='cursor-after-checked-value',block=block))
    assert results, 'No checked runtime output loads inspected'
    return results


def main():
    tool,cmake,root,directory,settings,config,suffix=sys.argv[1:]
    source,work=pathlib.Path(root),pathlib.Path(directory)
    evidence=load(source,'facts',work)
    types=run([tool,'--print-types',str(work/'answer.erl'),str(work/'client.erl')])
    for label in ['constant_plain','constant_spec']:
        assert re.search(r'function "'+label+r'"/0[^\n]*result=42',types),types
    for label in ['identity_plain','identity_spec']:
        assert re.search(r'function "'+label+r'"/1[^\n]*argument\[0\]',types),types
    for label in ['projected_plain','projected_spec']:
        assert re.search(r'function "'+label+r'"/2[^\n]*argument\[1\]',types),types
    for label in ['joined_plain','joined_spec','extracted_plain','record_spec']:
        line=next(line for line in types.splitlines() if 'function "'+label+'"/' in line)
        assert 'argument[' not in line,line
    checks=[]
    for triple in ['x86_64-pc-windows-msvc','i686-pc-windows-msvc']:
        for flags in [['-O0'],['-O2'],['-O0','--no-type-specialization'],['-O2','--no-type-specialization']]:
            for mode in ['--print-ir','--print-optimized-ir']:
                ir=run([tool,*flags,mode,'--target-triple',triple,str(work/'answer.erl'),str(work/'client.erl')])
                assert 'define i'+('64' if triple.startswith('x86_64') else '32') in ir
                if mode=='--print-ir' and triple.startswith('x86_64'): checks.extend(dominance(ir))
    native(tool,cmake,source,work,settings,config,suffix)
    evidence['dominance_checks']=checks
    (work/'evidence.json').write_bytes((json.dumps(evidence,indent=2)+'\n').encode())
    print(f'{evidence["calls"]} paired wrong-spec outcomes in four policies; types/both IR modes at two target widths; {len(checks)} checked-load dominance observations.')

if __name__=='__main__': main()

