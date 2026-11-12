"""Check sealed private original evidence and export only portable packet members."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import packet as P
import validate as V
from capture_reference import private_output

PRIVATE_SEAL='ac21a100f0d948d8d0e77f5d0addbd38a5fa58015bd6369bddd2b06e34b2dd07'

def checked_private(evidence):
    evidence=Path(evidence).resolve(strict=True);seal=evidence/'provenance.json'
    V.require(V.digest(seal)==PRIVATE_SEAL,'exact historical private seal')
    proof=V.load(seal)
    for name,pin in proof['manifest'].items():
        member=(evidence/name).resolve(strict=True)
        V.require(member.is_relative_to(evidence) and member.stat().st_size==pin['bytes'] and V.digest(member)==pin['sha256'],'sealed private member '+name)
    capture=V.load(evidence/'capture-2/capture.json')
    V.require(capture['status']=='PASS' and capture['input_before_sha256']==capture['input_after_sha256'] and capture['helper_before_sha256']==capture['helper_after_sha256'],'historical process/input/helper continuity')
    pinned=dict(V.load(evidence/'source-pins.json')['classes']);pinned.update(V.load(HERE/'helper-pins.json')['classes'])
    results=[]
    for vm in range(2):
        folder=evidence/'capture-2'/f'vm-{vm}';text=(folder/'target.stdout').read_text(encoding='utf-8')
        V.source_bindings(text,V.load(evidence/'source-pins.json'));raw=V.raw_rows(text)
        js=(folder/'journal.jsonl').read_text(encoding='utf-8');clean,code=V.journal(js)
        for record in (V.loads(line) for line in js.splitlines()):
            if record['event']=='journal':V.audit_journal(record,pinned,raw=True)
        result=V.project(raw,clean,code);V.validate_canonical(result);results.append(result)
    original=V.load(evidence/'capture-2/finite.json')
    V.require(V.digest(evidence/'capture-2/finite.json')==P.FINITE and V.exact(results[0],results[1]) and V.exact(results[0],original),'two raw original finite projections and immutable finite bytes')
    return original

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    # This check deliberately precedes evidence reads and all directory creation.
    out=private_output(a.output)
    original=checked_private(a.evidence);reference=P.read_reference();V.require(V.exact(original,reference),'current canonical export equals sealed actual observations')
    out.mkdir(parents=True,exist_ok=False)
    for name in (*P.DATA,*P.SOURCES,'provenance.json'):(out/name).write_bytes((HERE/name).read_bytes())
    V.require(V.exact(P.read_reference(out),original),'portable exported packet verifies independently')
    print('Portable original-only packet exported; no fresh original/native qualification:',out)

if __name__=='__main__':main()
