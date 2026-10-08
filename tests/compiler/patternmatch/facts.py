"""Compare paired misleading contracts and conservative binding facts through real native workflows."""
import json
import pathlib
import re
import sys
from matrix import option_lists
from evidence import run
from immediate import native
from stored import load


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
        assert re.search(r'%% inferred: '+label+r'\(\) -> 42',types),types
    for label in ['identity_plain','identity_spec']:
        assert re.search(r'%% inferred: '+label+r'\([^\n]*\) -> argument 1',types),types
    for label in ['projected_plain','projected_spec']:
        assert re.search(r'%% inferred: '+label+r'\([^\n]*\) -> argument 2',types),types
    for label in ['joined_plain','joined_spec','extracted_plain','record_spec']:
        line=next(line for line in types.splitlines() if line.startswith('%% inferred: '+label+'('))
        assert 'argument' not in line,line
    checks=[]
    for triple in ['x86_64-pc-windows-msvc','i686-pc-windows-msvc']:
        for flags in dict.fromkeys(tuple(flags) for flags,_ in option_lists()):
            for mode in ['--print-ir','--print-optimized-ir']:
                ir=run([tool,*flags,mode,'--target-triple',triple,str(work/'answer.erl'),str(work/'client.erl')])
                assert 'define i'+('64' if triple.startswith('x86_64') else '32') in ir
                if mode=='--print-ir' and triple.startswith('x86_64'): checks.extend(dominance(ir))
    native(tool,cmake,source,work,settings,config,suffix)
    evidence['dominance_checks']=checks
    (work/'evidence.json').write_bytes((json.dumps(evidence,indent=2)+'\n').encode())
    print(f'{evidence["calls"]} paired wrong-spec outcomes in four policies; types/both IR modes at two target widths; {len(checks)} checked-load dominance observations.')

if __name__=='__main__': main()

